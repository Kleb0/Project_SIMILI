let lastServerLogCount = 0;
let logBuffer = [];
let sessionInitialized = false;
let sentLogsToIframes = new Map(); 

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
        .then(data => {
            console.log('[Main Layout] Server connection established');
        })
        .catch(error => {
            console.error('[Main Layout] Failed to connect to server:', error);
        });
    }
}

window.addEventListener('DOMContentLoaded', () => {
    console.log('[Main Layout] Starting centralized log management');
    initializeServerConnection();
    setInterval(pollServerLogs, 500);
});
