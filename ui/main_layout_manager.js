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

window.addEventListener('message', (event) => {
    if (event.data.type === 'REQUEST_LOGS') {
        sendBufferedLogs(event.source);
    } else if (event.data.type === 'CLEAR_LOGS') {
        logBuffer = [];
        lastServerLogCount = 0;
        fetch('http://localhost:8080/console/clear', { method: 'POST' })
            .catch(err => console.error('Failed to clear server logs:', err));
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

function collectUIPanelIFrames() {
    const iframes = document.querySelectorAll('.main-container iframe');
    const iframeData = [];

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

    return iframeData;
}

function sendUIPanelIFramesToServer() {
    const iframeData = collectUIPanelIFrames();

    if (iframeData.length === 0) {
        return;
    }

    fetch('http://localhost:8080/api/uipanels/update', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({ iframes: iframeData })
    }).catch(() => {});
}

function scheduleUIPanelIFramesSync() {
    if (uiPanelSyncTimeout) {
        clearTimeout(uiPanelSyncTimeout);
    }

    uiPanelSyncTimeout = setTimeout(() => {
        sendUIPanelIFramesToServer();
    }, 120);
}

window.addEventListener('DOMContentLoaded', () => {
    initializeServerConnection();
    sendUIPanelIFramesToServer();
    setTimeout(sendUIPanelIFramesToServer, 250);
    setInterval(pollServerLogs, 500);
});

window.addEventListener('load', () => {
    scheduleUIPanelIFramesSync();
});

window.addEventListener('resize', () => {
    scheduleUIPanelIFramesSync();
});

window.sendUIPanelIFramesToServer = sendUIPanelIFramesToServer;
