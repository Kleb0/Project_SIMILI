let lastServerLogCount = 0;
let logBuffer = [];
let sessionInitialized = false;
let sentLogsToIframes = new Map(); 
let uiPanelSyncTimeout = null;

async function pollServerLogs() {
    try {
        const response = await fetch('http://localhost:8080/console/logs');
        if (response.ok) {
            const data = await response.json();
            if (data.logs && Array.isArray(data.logs)) {
                // Only process NEW logs
                const newLogs = data.logs.slice(lastServerLogCount);
                if (newLogs.length > 0) {

                    logBuffer.push(...newLogs);
                    lastServerLogCount = data.logs.length;
                    
                    broadcastLogsToIframes(newLogs);
                }
            }
        }
    } 
        catch (error) {
    }
}

function broadcastLogsToIframes(logs) {
    const iframes = document.querySelectorAll('iframe');
    iframes.forEach(iframe => {
        try {
            iframe.contentWindow.postMessage({
                type: 'SERVER_LOGS',
                logs: logs
            }, '*');
        } catch (e) {
        }
    });
}

function sendBufferedLogs(targetWindow) {
    if (logBuffer.length > 0) {
        targetWindow.postMessage({
            type: 'SERVER_LOGS_INIT',
            logs: logBuffer
        }, '*');
    }
}

window.addEventListener('message', async (event) => {
    if (event.data.type === 'REQUEST_LOGS') {
        sendBufferedLogs(event.source);
    } else if (event.data.type === 'CLEAR_LOGS') {
        logBuffer = [];
        lastServerLogCount = 0;
        fetch('http://localhost:8080/console/clear', { method: 'POST' })
            .catch(err => console.error('Failed to clear server logs:', err));
    } else if (event.data.type === 'SWITCH_WORKSPACE') {
        const workspace = event.data.workspace;
        
        // Notify backend of the active workspace, clear everything, and navigate
        try {
            await fetch('http://localhost:8080/api/workspace/set', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ workspace: workspace })
            });
            await fetch('http://localhost:8080/api/Endpoint/WorkspaceClear', { method: 'POST' });
        } catch (e) {
            console.error('Failed to clear panel map on layout change:', e);
        }

        if (workspace === 'drawing') {
            window.location.href = 'http://localhost:8080/ui/Drawing_board/drawing_board_layout.html';
        } else if (workspace === '3d') {
            window.location.href = 'http://localhost:8080/ui/main_layout.html';
        }
    }
});

function initializeServerConnection() {
    if (!sessionInitialized) {
        sessionInitialized = true;
        fetch('http://localhost:8080/console/test', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ message: 'Console frontend connected' })
        })
        .then(response => response.json())
    }
}

// think to use a for loop to get the iframes procedurally instead of hack-hardcoding the names
function getUIPanelName(iframe) {
    if (!iframe) {
        return null;
    }

    const src = iframe.getAttribute('src') || '';
    if (src.includes('Viewport_docking.html')) {
        return null;
    }

    const parentPanel = iframe.parentElement;
    if (!parentPanel) {
        return null;
    }

    const panelClasses = parentPanel.classList;

    for (let index = 0; index < panelClasses.length; index++) {
        const className = panelClasses[index];

        if (!className || className === 'panel') {
            continue;
        }

        return className.replace(/-/g, '_');
    }

    return null;
}

function computeLayoutIndices(panels) {
   
    const container = document.querySelector('.main-container');
   
    if (!container || panels.length === 0) {
        return;
    }

    const containerWidth = container.getBoundingClientRect().width;
    
    if (containerWidth <= 0) {
        return;
    }

    const fullWidthThreshold = 0.85;
    const rowPanels = [];
    const columnPanels = [];

    panels.forEach(panel => {
        if (panel.width <= 0 || panel.height <= 0) {
            return;
        }
        const widthRatio = panel.width / containerWidth;
        if (widthRatio >= fullWidthThreshold) {
            rowPanels.push(panel);
        } else {
            columnPanels.push(panel);
        }
    });

    rowPanels.sort((a, b) => b.y - a.y);

    columnPanels.sort((a, b) => {
        const xDiff = a.x - b.x;
        if (Math.abs(xDiff) > 10) {
            return xDiff;
        }
        return a.y - b.y;
    });

    let index = 1;
    rowPanels.forEach(panel => { panel.layoutIndex = index++; });
    columnPanels.forEach(panel => { panel.layoutIndex = index++; });
}

function collectUIPanelIFrames() {
    const iframes = document.querySelectorAll('.main-container iframe');
    const iframeData = [];

    fetch('http://localhost:8080/api/debug/iframe-count', { 
        method: 'POST', 
        body: 'Found ' + iframes.length + ' iframes in DOM' 
    }).catch(() => {});

    iframes.forEach(iframe => {
        const panelName = getUIPanelName(iframe);
        if (!panelName) {
            return;
        }

        const iframeRect = iframe.getBoundingClientRect();

        iframeData.push({
            name: panelName,
            x: Math.round(iframeRect.left),
            y: Math.round(iframeRect.top),
            width: Math.round(iframeRect.width),
            height: Math.round(iframeRect.height),
            clientX: Math.round(iframeRect.left),
            clientY: Math.round(iframeRect.top)
        });
    });

    computeLayoutIndices(iframeData);

    return iframeData;
}

function sendUIPanelIFramesToServer() {
    let iframeData = collectUIPanelIFrames();
    const isDrawingWorkspace = window.location.href.includes('drawing_board_layout.html');
    const endpoint = isDrawingWorkspace
        ? 'http://localhost:8080/api/Endpoint/DrawingScreenPanels'
        : 'http://localhost:8080/api/Endpoint/ThreeDScenePanels';

    // In Drawing Workspace, send all panels (top_bar + drawing-specific ones)
    // No filter needed — DrawingScreenPanels route handles all of them

    fetch('http://localhost:8080/api/debug/panels-found', { 
        method: 'POST', 
        body: 'Collected ' + iframeData.length + ' panels' 
    }).catch(() => {});

    if (iframeData.length === 0) {
        fetch('http://localhost:8080/api/debug/no-panels', { method: 'POST', body: 'No panels to send' }).catch(() => {});
        return;
    }

    fetch(endpoint, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({ iframes: iframeData })
    }).then(response => {
        fetch('http://localhost:8080/api/debug/send-success', { 
            method: 'POST', 
            body: 'Panels sent successfully: ' + response.status 
        }).catch(() => {});
    }).catch(error => {
        fetch('http://localhost:8080/api/debug/send-error', { 
            method: 'POST', 
            body: 'Send failed: ' + error.toString() 
        }).catch(() => {});
    });
}

function scheduleUIPanelIFramesSync() {
    if (uiPanelSyncTimeout) {
        clearTimeout(uiPanelSyncTimeout);
    }

    uiPanelSyncTimeout = setTimeout(() => {
        sendUIPanelIFramesToServer();
    }, 120);
}


// Execute immediately instead of waiting for DOMContentLoaded (CEF issue)
fetch('http://localhost:8080/api/debug/init-start', { method: 'POST', body: 'Init start from main_layout_manager' }).catch(() => {});
initializeServerConnection();
setInterval(pollServerLogs, 1000);

window.addEventListener('load', () => {
    const isDrawingWorkspace = window.location.href.includes('drawing_board_layout.html');
    const activeWorkspace = isDrawingWorkspace ? 'drawing' : '3d';
    
    // Automatically sync initial workspace to C++ server on load
    fetch('http://localhost:8080/api/workspace/set', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ workspace: activeWorkspace })
    }).catch(e => console.error('Failed to sync initial workspace to server:', e));

    if (isDrawingWorkspace) {
        // Send all drawing workspace panels to the server
        setTimeout(() => {
            sendUIPanelIFramesToServer();
        }, 300);
    } else {
        setTimeout(() => {
            sendUIPanelIFramesToServer();
        }, 500);
    }

    setTimeout(() => {
        if (typeof sendAllPanelFieldPositions === 'function') sendAllPanelFieldPositions();
    }, 600);
    
    // Sync buttons UI status in Top Bar iframe
    setTimeout(() => {
        const topBarIframe = document.querySelector('.top-bar-panel iframe');
        if (topBarIframe && topBarIframe.contentWindow) {
            topBarIframe.contentWindow.postMessage({
                type: 'SYNC_WORKSPACE_BUTTONS',
                workspace: activeWorkspace
            }, '*');
        }
    }, 650);
});

window.addEventListener('resize', () => {
    if (typeof sendAllPanelFieldPositions === 'function') sendAllPanelFieldPositions();
});

window.addEventListener('resize', () => {
    scheduleUIPanelIFramesSync();
});

window.sendUIPanelIFramesToServer = sendUIPanelIFramesToServer;

function pollDataHolderValues() {
    var panelConfigs = [
        { panelName: 'object_inspector_panel', iframeSelector: '.object-inspector-panel iframe' }
    ];

    panelConfigs.forEach(function(cfg) {
        fetch('http://localhost:8080/api/dataholder/values?panel=' + cfg.panelName)
            .then(function(r) { return r.ok ? r.json() : null; })
            .then(function(values) {
                if (!values) return;
                var iframe = document.querySelector(cfg.iframeSelector);
                if (!iframe) return;
                try {
                    var doc = iframe.contentDocument || iframe.contentWindow.document;
                    if (!doc) return;
                    Object.keys(values).forEach(function(field) {
                        var el = doc.querySelector('[data-field="' + field + '"]');
                        if (!el) return;
                        if (el.tagName === 'INPUT') {
                            if (el.value !== values[field]) el.value = values[field];
                        } else {
                            if (el.textContent !== values[field]) el.textContent = values[field];
                        }
                    });
                } catch(e) {}
            })
            .catch(function() {});
    });
}

setInterval(pollDataHolderValues, 500);
