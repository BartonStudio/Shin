import re
import codecs

filepath = 'third_party/HCSDK/include/HCNetSDK.h'

try:
    print(f"Step 1: Reading {filepath}")
    with codecs.open(filepath, 'r', encoding='gbk', errors='replace') as f:
        content = f.read()
    
    print("Step 2: Stripping comments")
    # Multi-line first
    content = re.sub(r'/\*.*?\*/', ' ', content, flags=re.DOTALL)
    # Single-line
    content = re.sub(r'//.*', ' ', content)
    
    print("Step 3: Removing non-ASCII characters")
    clean_content = "".join([c if ord(c) < 128 else ' ' for c in content])
    
    print("Step 4: Writing clean UTF-8 content")
    with codecs.open(filepath, 'w', encoding='utf-8') as f:
        f.write(clean_content)
    
    print("Step 5: Cleanup successful")
except Exception as e:
    print(f"Error: {e}")
