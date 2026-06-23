// Layout Resizer - Makes splitters draggable
(function() {
    'use strict';

    let isDragging = false;
    let currentSplitter = null;
    let startPos = 0;
    let startSizes = [];
    let resizeTimeout = null;
    let isWindowMaximized = false;
    let hasDragged = false;

    function initSplitters() {
        const splitters = document.querySelectorAll('.splitter');
        
        splitters.forEach(splitter => {
            splitter.addEventListener('mousedown', handleMouseDown);
        });

        document.addEventListener('mousemove', handleMouseMove);
        document.addEventListener('mouseup', handleMouseUp);
        
        window.addEventListener('resize', handleWindowResize);
        
        checkWindowState();
    }
    
    function checkWindowState() 
    {
        const isMax = (window.innerWidth >= screen.width - 20 && window.innerHeight >= screen.height - 100);
        if (isMax !== isWindowMaximized) {
            isWindowMaximized = isMax;
        }
    }

    function handleWindowResize() {
        // Check window state change
        checkWindowState();
        
        // Notify viewport resize immediately
        notifyViewportResize();
        
        // Debounce resize events
        if (resizeTimeout) {
            clearTimeout(resizeTimeout);
        }
        
        resizeTimeout = setTimeout(() => {
            // Reset horizontal sections (left, center, right)
            const sections = document.querySelectorAll('.left-section, .center-section, .right-section');
            sections.forEach(section => {
                section.style.width = '';
                section.style.flex = '';
            });
            
            // Reset vertical sections (top-row, project-viewer)
            const topRow = document.querySelector('.top-row');
            const projectViewer = document.querySelector('.project-viewer-panel');
            if (topRow) {
                topRow.style.height = '';
                topRow.style.flex = '';
            }
            if (projectViewer) {
                projectViewer.style.height = '';
                projectViewer.style.flex = '';
            }
            
            // Reset all panels inside sections
            const panels = document.querySelectorAll('.panel');
            panels.forEach(panel => {
                panel.style.height = '';
                panel.style.flex = '';
            });
            
            // Notify again after reset
            notifyViewportResize();
            
            // Send all iframe sizes to server after resize
            sendIFrameSizesToServer();
            if (window.sendDataHoldersToServer) {
                window.sendDataHoldersToServer();
            }
        }, 150); // Wait 150ms after last resize event
    }

    function handleMouseDown(e) {
        e.preventDefault();
        
        isDragging = true;
        hasDragged = true;
        currentSplitter = e.target;
        
        const direction = currentSplitter.getAttribute('data-direction');
        
        if (direction === 'horizontal') {
            // Vertical splitter (horizontal resize)
            startPos = e.clientX;
            const prevElement = currentSplitter.previousElementSibling;
            const nextElement = currentSplitter.nextElementSibling;
            
            if (prevElement && nextElement) {
                startSizes = [
                    prevElement.offsetWidth,
                    nextElement.offsetWidth
                ];
            }
            document.body.classList.add('dragging');
        } else if (direction === 'vertical') {
            // Horizontal splitter (vertical resize)
            startPos = e.clientY;
            const prevElement = currentSplitter.previousElementSibling;
            const nextElement = currentSplitter.nextElementSibling;
            
            if (prevElement && nextElement) {
                startSizes = [
                    prevElement.offsetHeight,
                    nextElement.offsetHeight
                ];
            }
            document.body.classList.add('dragging-vertical');
        }
        
        // Disable pointer events on iframes during drag
        document.querySelectorAll('iframe').forEach(iframe => {
            iframe.style.pointerEvents = 'none';
        });
    }

    function handleMouseMove(e) {
        if (!isDragging || !currentSplitter) return;
        
        e.preventDefault();
        
        const direction = currentSplitter.getAttribute('data-direction');
        const prevElement = currentSplitter.previousElementSibling;
        const nextElement = currentSplitter.nextElementSibling;
        
        if (!prevElement || !nextElement) return;
        
        if (direction === 'horizontal') {
            // Vertical splitter (horizontal resize)
            const delta = e.clientX - startPos;
            let newPrevWidth = startSizes[0] + delta;
            let newNextWidth = startSizes[1] - delta;
            
            // Set minimum widths (150px for side panels, 300px for center)
            const minPrevWidth = prevElement.classList.contains('center-section') ? 300 : 150;
            const minNextWidth = nextElement.classList.contains('center-section') ? 300 : 150;
            
            // Clamp to minimums
            if (newPrevWidth < minPrevWidth) {
                const diff = minPrevWidth - newPrevWidth;
                newPrevWidth = minPrevWidth;
                newNextWidth -= diff;
            }
            if (newNextWidth < minNextWidth) {
                const diff = minNextWidth - newNextWidth;
                newNextWidth = minNextWidth;
                newPrevWidth -= diff;
            }
            
            if (newPrevWidth >= minPrevWidth && newNextWidth >= minNextWidth) {
                // Use fixed pixel widths for precise control
                prevElement.style.width = newPrevWidth + 'px';
                prevElement.style.flex = '0 0 auto';
                nextElement.style.width = newNextWidth + 'px';
                nextElement.style.flex = '0 0 auto';
                
                // Force browser repaint
                forceBrowserRepaint();
            }
        } else if (direction === 'vertical') {
            // Horizontal splitter (vertical resize)
            const delta = e.clientY - startPos;
            let newPrevHeight = startSizes[0] + delta;
            let newNextHeight = startSizes[1] - delta;
            
            // Determine minimum heights based on element type
            let minPrevHeight = 100;
            let minNextHeight = 100;
            
            // If prev element is top-row, it needs more space
            if (prevElement.classList.contains('top-row')) {
                minPrevHeight = 200;
            }
            
            // Clamp to minimums
            if (newPrevHeight < minPrevHeight) {
                const diff = minPrevHeight - newPrevHeight;
                newPrevHeight = minPrevHeight;
                newNextHeight -= diff;
            }
            if (newNextHeight < minNextHeight) {
                const diff = minNextHeight - newNextHeight;
                newNextHeight = minNextHeight;
                newPrevHeight -= diff;
            }
            
            if (newPrevHeight >= minPrevHeight && newNextHeight >= minNextHeight) {
                // Check if this is the main splitter (between top-row and project-viewer)
                const isMainSplitter = prevElement.classList.contains('top-row');
                
                if (isMainSplitter) {
                    // Main splitter: control top-row and project-viewer heights
                    prevElement.style.height = newPrevHeight + 'px';
                    prevElement.style.flex = '0 0 auto';
                    nextElement.style.height = newNextHeight + 'px';
                    nextElement.style.flex = '0 0 auto';
                } else {
                    // Internal splitter (e.g., in right-section): only affect panels
                    prevElement.style.height = newPrevHeight + 'px';
                    prevElement.style.flex = '0 0 auto';
                    nextElement.style.height = newNextHeight + 'px';
                    nextElement.style.flex = '0 0 auto';
                }
                
                // Force browser repaint
                forceBrowserRepaint();
            }
        }
    }
    
    function forceBrowserRepaint() {
        // Notify C++ to update viewport overlay dimensions
        notifyViewportResize();
        
        // Force reflow by accessing offsetHeight (lightweight)
        document.body.offsetHeight;
    }
    
    function notifyViewportResize() 
    {
        const viewportPanel = document.querySelector('.viewport-panel');
        if (viewportPanel) {
            const rect = viewportPanel.getBoundingClientRect();
                       
            const borderWidth = 0;
            
            const adjustedX = Math.round(rect.left + borderWidth);
            const adjustedY = Math.round(rect.top + borderWidth);
            
            const adjustedWidth = Math.round(rect.width - (2 * borderWidth));
            const adjustedHeight = Math.round(rect.height - (2 * borderWidth));

            const dpiScale = window.devicePixelRatio || 1.0;
            
            const message = 'VIEWPORT_RESIZE:' + 
                adjustedX + ',' + 
                adjustedY + ',' + 
                adjustedWidth + ',' + 
                adjustedHeight + ',' +
                dpiScale;
            
            document.title = message;
        }
    }

    function handleMouseUp(e) 
    {
        if (hasDragged)
        {
            sendIFrameSizesToServer();
            // First call at 150ms (quick update)
            setTimeout(function() {
                if (window.sendDataHoldersToServer) {
                    window.sendDataHoldersToServer();
                }
            }, 150);
            // Second call at 450ms (after CSS reflow is stable in CEF)
            setTimeout(function() {
                if (window.sendDataHoldersToServer) {
                    window.sendDataHoldersToServer();
                }
            }, 450);
            hasDragged = false;
        }
        if (!isDragging) return;
        
        isDragging = false;
        currentSplitter = null;
        
        document.body.classList.remove('dragging');
        document.body.classList.remove('dragging-vertical');
        
        document.querySelectorAll('iframe').forEach(iframe => {
            iframe.style.pointerEvents = 'auto';
        });
        
        forceBrowserRepaint();
        
        if (hasDragged) {
            sendIFrameSizesToServer();
            hasDragged = false;
        }
    }
    
    function sendIFrameSizesToServer()
    {
        const isDrawingWorkspace = window.location.href.includes('drawing_board_layout.html');
        const iframes = document.querySelectorAll('.main-container iframe');
        const iframeData = [];
        
        iframes.forEach(iframe => {
            const src = iframe.getAttribute('src') || '';
            
            // Get panel name from parent class
            const parentPanel = iframe.parentElement;
            if (!parentPanel) return;
            
            let name = null;
            const panelClasses = parentPanel.classList;
            for (let index = 0; index < panelClasses.length; index++) {
                const className = panelClasses[index];
                if (!className || className === 'panel') continue;
                name = className.replace(/-/g, '_');
            }
            if (!name) return;

            // In Drawing Workspace, only allow top_bar_panel through
            if (isDrawingWorkspace && name !== 'top_bar_panel') return;

            const iframeRect = iframe.getBoundingClientRect();
            const logicalX = Math.round(iframeRect.left);
            const logicalY = Math.round(iframeRect.top);
            const logicalWidth = Math.round(iframeRect.width);
            const logicalHeight = Math.round(iframeRect.height);
            
            iframeData.push({
                name: name,
                x: logicalX,
                y: logicalY,
                width: logicalWidth,
                height: logicalHeight,
                clientX: logicalX + 40,
                clientY: logicalY + 50,
                marginLeft: 40,
                marginRight: 40,
            });
        });
        
        if (iframeData.length > 0) {
            fetch('http://localhost:8080/api/iframes/update', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({ iframes: iframeData })
            }).catch(() => {});
        }
    }

    function syncIFrameSizesToServer()
    {
        notifyViewportResize();
        sendIFrameSizesToServer();
    }

    function runStartupSync()
    {
        syncIFrameSizesToServer();

        window.requestAnimationFrame(function() {
            syncIFrameSizesToServer();
        });

        setTimeout(function() {
            syncIFrameSizesToServer();
        }, 100);

        setTimeout(function() {
            syncIFrameSizesToServer();
        }, 300);
    }

    window.notifyViewportResize = notifyViewportResize;
    window.sendIFrameSizesToServer = sendIFrameSizesToServer;
    window.syncIFrameSizesToServer = runStartupSync;

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', function() {
            initSplitters();
            runStartupSync();
        });
    } else {
        initSplitters();
        runStartupSync();
    }
    
    window.addEventListener('load', function() {
        runStartupSync();
    });
})();
