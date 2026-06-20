(function() {
    function initMenu() {
        if (!document.body) { setTimeout(initMenu, 100); return; }

        const style = document.createElement('style');
        style.innerHTML = `
            #shin-dev-menu-container { position: fixed; top: 10px; right: 10px; z-index: 9999; }
            #shin-dev-menu { 
                display: none; flex-direction: column; gap: 5px; 
                background: rgba(0,0,0,0.8); color: white; padding: 10px; border-radius: 5px; font-family: sans-serif; font-size: 12px;
            }
            #shin-dev-menu input { width: 150px; }
        `;
        document.head.appendChild(style);

        const container = document.createElement('div');
        container.id = 'shin-dev-menu-container';
        container.innerHTML = `
            <button id="toggle-menu">DevMenu</button>
            <div id="shin-dev-menu">
                <button onclick="window.Shin.sendDataToCpp(JSON.stringify({action:'WindowOpenDevTools'}))">Open DevTools</button>
                <button onclick="window.Shin.sendDataToCpp(JSON.stringify({action:'ZoomIn'}))">Zoom +</button>
                <button onclick="window.Shin.sendDataToCpp(JSON.stringify({action:'ZoomOut'}))">Zoom -</button>
                <button onclick="window.Shin.sendDataToCpp(JSON.stringify({action:'WindowClose'}))">Close App</button>
                <input type="text" id="nav-url" placeholder="https://...">
                <button onclick="window.Shin.sendDataToCpp(JSON.stringify({action:'Navigate', url:document.getElementById('nav-url').value}))">Go</button>
            </div>
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
