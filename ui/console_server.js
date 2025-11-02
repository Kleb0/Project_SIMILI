function navigateToPage(page) {
    // No longer saving state - parent manages persistence
    window.location.href = page;
}

const consoleOutput = document.getElementById('consoleOutput');
let logCounter = 0;
let userHasScrolled = false;

// Detect if user scrolled up manually
consoleOutput.addEventListener('scroll', () => {
    const isAtBottom = consoleOutput.scrollHeight - consoleOutput.scrollTop <= consoleOutput.clientHeight + 50;
    userHasScrolled = !isAtBottom;
});

// Override console.log to capture JavaScript logs
const originalConsoleLog = console.log;
console.log = function(...args) {
    // Filter out known spam messages
    const message = args.map(arg => 
        typeof arg === 'object' ? JSON.stringify(arg, null, 2) : String(arg)
    ).join(' ');
    
    // Don't log if it's just the server response from connection test
    if (!message.includes('Server response:')) {
        originalConsoleLog.apply(console, args);
        addLogEntry('[JS] ' + message, 'js');
    }
};

// Override console.error
const originalConsoleError = console.error;
console.error = function(...args) {
    const message = args.map(arg => 
        typeof arg === 'object' ? JSON.stringify(arg, null, 2) : String(arg)
    ).join(' ');
    
    // Don't log connection test errors
    if (!message.includes('Failed to connect to server')) {
        originalConsoleError.apply(console, args);
        addLogEntry('[JS ERROR] ' + message, 'error');
    }
};

function addLogEntry(message, type = 'info') {
    const timestamp = new Date().toLocaleTimeString('fr-FR', { 
        hour12: false, 
        hour: '2-digit', 
        minute: '2-digit', 
        second: '2-digit',
        fractionalSecondDigits: 3
    });
    
    const logEntry = document.createElement('div');
    logEntry.className = `log-entry log-${type}`;
    logEntry.innerHTML = `<span class="timestamp">[${timestamp}]</span> ${escapeHtml(message)}`;
    
    consoleOutput.appendChild(logEntry);
    
    // Only auto-scroll if user hasn't manually scrolled up
    if (!userHasScrolled) {
        consoleOutput.scrollTop = consoleOutput.scrollHeight;
    }
    
    logCounter++;
}

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

function clearConsole() {
    consoleOutput.innerHTML = '';
    logCounter = 0;
    addLogEntry('Console cleared', 'info');
    
    // Notify parent to clear server logs
    if (window.parent !== window) {
        window.parent.postMessage({ type: 'CLEAR_LOGS' }, '*');
    }
}

// Send test message to server
async function sendTestMessage() {
    try {
        addLogEntry('[JavaScript] Sending: Hello From Javascript', 'info');
        
        const response = await fetch('http://localhost:8080/hello', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ message: 'Hello From Javascript' })
        });
        
        if (response.ok) {
            const data = await response.json();
            addLogEntry('[JavaScript] Server response: ' + JSON.stringify(data), 'info');
        } else {
            addLogEntry('[JavaScript] Server error: ' + response.status, 'error');
        }
    } catch (error) {
        addLogEntry('[JavaScript] Failed to send message: ' + error.message, 'error');
    }
}

function saveConsoleState() {
    // No longer needed - parent window manages persistence
    // Keeping function for compatibility but doing nothing
}

function restoreConsoleState() {
    // No longer needed - logs come from parent window
    // Always return false to indicate no restore happened
    return false;
}

// Poll server for logs every 500ms
// REMOVED - Now handled by parent window (main_layout.html)
// Logs are received via postMessage

// Listen for logs from parent window
let initialLogsSent = false; // Track if we already received initial batch

window.addEventListener('message', (event) => {
    if (event.data.type === 'SERVER_LOGS_INIT') {
        // Initial batch of all logs when page loads (only process once)
        if (!initialLogsSent) {
            initialLogsSent = true;
            event.data.logs.forEach(log => {
                addLogEntry(log.message, log.type || 'server');
            });
        }
    } else if (event.data.type === 'SERVER_LOGS') {
        // New logs from server (always process these)
        // Skip if we haven't received initial batch yet to avoid race condition
        if (initialLogsSent) {
            event.data.logs.forEach(log => {
                addLogEntry(log.message, log.type || 'server');
            });
        }
    }
});

// Test connection on load
window.addEventListener('DOMContentLoaded', () => {
    // No longer restoring from localStorage - logs come from parent
    addLogEntry('Console initialized', 'info');
    
    // Request all logs from parent window
    if (window.parent !== window) {
        window.parent.postMessage({ type: 'REQUEST_LOGS' }, '*');
    }
});

// No longer saving state on unload - parent manages persistence
