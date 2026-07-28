"""ShinCalendar - SQLite-based calendar/memo manager with TOML schema support."""

import argparse
import sqlite3
import tomllib


def init_db(schema_path: str, db_path: str):
    """Create SQLite database from TOML schema definition."""
    with open(schema_path, "rb") as f:
        schema = tomllib.load(f)

    conn = sqlite3.connect(db_path)
    conn.execute("PRAGMA foreign_keys = ON")
    cursor = conn.cursor()

    for table_name, table_def in schema["tables"].items():
        col_defs = []
        table_constraints = []
        for col in table_def["columns"]:
            col_name = col["name"]
            col_type = col["type"]
            raw = col.get("constraints", "")
            # The 'constraints' pseudo-column holds table-level FK definitions
            if col_name == "constraints":
                for fk in col_type.split(","):
                    fk = fk.strip()
                    if fk.startswith("FOREIGN KEY"):
                        table_constraints.append(fk)
            else:
                parts = [col_name, col_type]
                if raw:
                    parts.append(raw)
                col_defs.append(" ".join(parts))

        all_parts = col_defs + table_constraints
        sql = f"CREATE TABLE IF NOT EXISTS {table_name} ({', '.join(all_parts)})"
        cursor.execute(sql)

    conn.commit()
    conn.close()
    print(f"Database initialized: {db_path}")


def add_memo(db_path: str, content: str, start: str, end: str | None = None, tags: str | None = None):
    """Add a memo with optional tags."""
    conn = sqlite3.connect(db_path)
    conn.execute("PRAGMA foreign_keys = ON")
    cursor = conn.cursor()

    cursor.execute(
        "INSERT INTO memos (content, start_time, end_time) VALUES (?, ?, ?)",
        (content, start, end),
    )
    memo_id = cursor.lastrowid

    if tags:
        for tag_name in tags.split(","):
            tag_name = tag_name.strip()
            if not tag_name:
                continue
            cursor.execute("INSERT OR IGNORE INTO tags (name) VALUES (?)", (tag_name,))
            cursor.execute("SELECT id FROM tags WHERE name = ?", (tag_name,))
            tag_id = cursor.fetchone()[0]
            cursor.execute(
                "INSERT INTO memo_tags (memo_id, tag_id) VALUES (?, ?)",
                (memo_id, tag_id),
            )

    conn.commit()
    print(f"Added memo #{memo_id}: {content} ({start} ~ {end})")
    if tags:
        print(f"  Tags: {tags}")
    conn.close()


def list_memos(db_path: str):
    """List all non-deleted memos with their tags."""
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()

    rows = cursor.execute(
        """SELECT m.id, m.content, m.start_time, m.end_time,
           GROUP_CONCAT(t.name, ', ') as tags
           FROM memos m
           LEFT JOIN memo_tags mt ON m.id = mt.memo_id
           LEFT JOIN tags t ON mt.tag_id = t.id
           WHERE m.is_deleted = 0
           GROUP BY m.id
           ORDER BY m.start_time"""
    ).fetchall()

    if not rows:
        print("No memos found.")
        conn.close()
        return

    print(f"{'ID':<4} {'Content':<30} {'Start':<12} {'End':<12} {'Tags'}")
    print("-" * 70)
    for row in rows:
        id, content, start, end, tags = row
        end_display = end or "-"
        tags_display = tags or "-"
        print(f"{id:<4} {content:<30} {start:<12} {end_display:<12} {tags_display}")

    conn.close()


def search_memos(db_path: str, keyword: str | None = None, tag: str | None = None,
                 from_date: str | None = None, to_date: str | None = None):
    """Search memos by keyword, tag, or date range."""
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()

    query = """
        SELECT m.id, m.content, m.start_time, m.end_time,
        GROUP_CONCAT(t2.name, ', ') as tags
        FROM memos m
        LEFT JOIN memo_tags mt ON m.id = mt.memo_id
        LEFT JOIN tags t2 ON mt.tag_id = t2.id
        WHERE m.is_deleted = 0
    """
    conditions = []
    params = []

    if keyword:
        conditions.append("m.content LIKE ?")
        params.append(f"%{keyword}%")

    if tag:
        conditions.append("""
            EXISTS (
                SELECT 1 FROM memo_tags mt2
                JOIN tags t3 ON mt2.tag_id = t3.id
                WHERE mt2.memo_id = m.id AND t3.name = ?
            )
        """)
        params.append(tag)

    if from_date:
        conditions.append("m.start_time >= ?")
        params.append(from_date)

    if to_date:
        conditions.append("m.start_time <= ?")
        params.append(to_date)

    if conditions:
        query += " AND " + " AND ".join(conditions)

    query += " GROUP BY m.id ORDER BY m.start_time"

    rows = cursor.execute(query, params).fetchall()

    if not rows:
        print("No matching memos found.")
        conn.close()
        return

    print(f"{'ID':<4} {'Content':<30} {'Start':<12} {'End':<12} {'Tags'}")
    print("-" * 70)
    for row in rows:
        id, content, start, end, tags = row
        end_display = end or "-"
        tags_display = tags or "-"
        print(f"{id:<4} {content:<30} {start:<12} {end_display:<12} {tags_display}")

    conn.close()


def delete_memo(db_path: str, memo_id: int, hard: bool = False):
    """Delete a memo (soft delete by default, hard delete with --hard)."""
    conn = sqlite3.connect(db_path)
    conn.execute("PRAGMA foreign_keys = ON")
    cursor = conn.cursor()

    cursor.execute("SELECT content FROM memos WHERE id = ?", (memo_id,))
    row = cursor.fetchone()
    if not row:
        print(f"Memo #{memo_id} not found.")
        conn.close()
        return

    content = row[0]

    if hard:
        cursor.execute("DELETE FROM memo_tags WHERE memo_id = ?", (memo_id,))
        cursor.execute("DELETE FROM memos WHERE id = ?", (memo_id,))
        print(f"Hard deleted memo #{memo_id}: {content}")
    else:
        cursor.execute("UPDATE memos SET is_deleted = 1 WHERE id = ?", (memo_id,))
        print(f"Soft deleted memo #{memo_id}: {content}")

    conn.commit()
    conn.close()


def main():
    parser = argparse.ArgumentParser(description="ShinCalendar - memo manager")
    subparsers = parser.add_subparsers(dest="command", required=True)

    # init
    init_parser = subparsers.add_parser("init", help="Initialize database from TOML schema")
    init_parser.add_argument("schema", help="TOML schema file path")
    init_parser.add_argument("db", help="Database file path")

    # add
    add_parser = subparsers.add_parser("add", help="Add a memo")
    add_parser.add_argument("db", help="Database file path")
    add_parser.add_argument("--content", required=True, help="Memo content")
    add_parser.add_argument("--start", required=True, help="Start date (YYYY-MM-DD)")
    add_parser.add_argument("--end", default=None, help="End date (YYYY-MM-DD)")
    add_parser.add_argument("--tags", default=None, help="Comma-separated tag names")

    # list
    list_parser = subparsers.add_parser("list", help="List all memos")
    list_parser.add_argument("db", help="Database file path")

    # search
    search_parser = subparsers.add_parser("search", help="Search memos")
    search_parser.add_argument("db", help="Database file path")
    search_parser.add_argument("--keyword", default=None, help="Search in content")
    search_parser.add_argument("--tag", default=None, help="Filter by tag name")
    search_parser.add_argument("--from", default=None, dest="from_date", help="Start date filter")
    search_parser.add_argument("--to", default=None, dest="to_date", help="End date filter")

    # delete
    delete_parser = subparsers.add_parser("delete", help="Delete a memo")
    delete_parser.add_argument("db", help="Database file path")
    delete_parser.add_argument("--id", required=True, type=int, help="Memo ID to delete")
    delete_parser.add_argument("--hard", action="store_true", help="Hard delete (remove from DB)")

    args = parser.parse_args()

    if args.command == "init":
        init_db(args.schema, args.db)
    elif args.command == "add":
        add_memo(args.db, args.content, args.start, args.end, args.tags)
    elif args.command == "list":
        list_memos(args.db)
    elif args.command == "search":
        search_memos(args.db, args.keyword, args.tag, args.from_date, args.to_date)
    elif args.command == "delete":
        delete_memo(args.db, args.id, args.hard)


if __name__ == "__main__":
    main()
