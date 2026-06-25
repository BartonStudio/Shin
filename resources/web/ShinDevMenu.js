(function() {
    function initMenu() {
        if (!document.body) { setTimeout(initMenu, 100); return; }

        const style = document.createElement('style');
        style.innerHTML = `
            #shin-dev-menu-container { 
                position: fixed; right: 0; top: 50%; transform: translateY(-50%); 
                z-index: 9999; display: flex; align-items: center;
            }
            #toggle-menu {
                width: 24px; height: 100px; background: #333; color: white;
                border: none; border-radius: 4px 0 0 4px; cursor: pointer;
                writing-mode: vertical-rl; text-orientation: mixed;
                font-size: 12px; display: flex; align-items: center; justify-content: center;
            }
            #toggle-menu:hover { background: #007acc; }
            #shin-dev-menu { 
                display: none; flex-direction: column; gap: 5px; 
                background: rgba(0,0,0,0.8); color: white; padding: 10px; border-radius: 5px 0 0 5px; 
                font-family: sans-serif; font-size: 12px; margin-right: 5px;
            }
            #shin-dev-menu input { width: 120px; }
        `;
        document.head.appendChild(style);

        const container = document.createElement('div');
        container.id = 'shin-dev-menu-container';
        container.innerHTML = `
            <div id="shin-dev-menu">
                <button onclick="window.Shin.sendDataToCpp(JSON.stringify({action:'WindowOpenDevTools'}))">Open DevTools</button>
                <button onclick="window.Shin.sendDataToCpp(JSON.stringify({action:'ZoomIn'}))">Zoom +</button>
                <button onclick="window.Shin.sendDataToCpp(JSON.stringify({action:'ZoomOut'}))">Zoom -</button>
                <button onclick="window.Shin.sendDataToCpp(JSON.stringify({action:'WindowClose'}))">Close App</button>
                <div style="display: flex; gap: 5px; margin-top: 5px;">
                    <input type="number" id="w-input" placeholder="W" style="width: 50px;">
                    <input type="number" id="h-input" placeholder="H" style="width: 50px;">
                    <button onclick="window.Shin.sendDataToCpp(JSON.stringify({
                        action: 'WindowSetSize', 
                        width: parseInt(document.getElementById('w-input').value), 
                        height: parseInt(document.getElementById('h-input').value),
                        fixed: false
                    }))">Resize</button>
                </div>
                <input type="text" id="nav-url" placeholder="https://...">
                <button onclick="window.Shin.sendDataToCpp(JSON.stringify({action:'Navigate', url:document.getElementById('nav-url').value}))">Go</button>
            </div>
            <button id="toggle-menu">DevMenu</button>
        `;
        document.body.appendChild(container);

        document.getElementById('toggle-menu').onclick = () => {
            const menu = document.getElementById('shin-dev-menu');
            menu.style.display = (menu.style.display === 'flex') ? 'none' : 'flex';
        };
    }

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', initMenu);
    } else {
        initMenu();
    }
})();
