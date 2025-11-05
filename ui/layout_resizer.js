// Layout Resizer - Makes splitters draggable
(function() {
    'use strict';

    let isDragging = false;
    let currentSplitter = null;
    let startPos = 0;
    let startSizes = [];
    let resizeTimeout = null;
    let isWindowMaximized = false;

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
        }, 150); // Wait 150ms after last resize event
    }

    function handleMouseDown(e) {
        e.preventDefault();
        
        isDragging = true;
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
            
            // Get the actual dimensions without any adjustment
            // getBoundingClientRect() already includes borders in the coordinates
            // The overlay should be positioned at the INNER area of the panel (excluding border)
            
            // Panel border: 3px on each side
            const borderWidth = 3;
            
            // Position: add border to get to inner area
            const adjustedX = Math.round(rect.left + borderWidth);
            const adjustedY = Math.round(rect.top + borderWidth);
            
            // Size: subtract borders from both sides
            const adjustedWidth = Math.round(rect.width - (2 * borderWidth));
            const adjustedHeight = Math.round(rect.height - (2 * borderWidth));
            
            // Get DPI scaling factor (devicePixelRatio)
            // e.g., 1.0 for 100%, 1.5 for 150%, 2.0 for 200%
            const dpiScale = window.devicePixelRatio || 1.0;
            
            // Send viewport dimensions by setting a special document title
            // Format: "VIEWPORT_RESIZE:x,y,width,height,dpiScale"
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
        if (!isDragging) return;
        
        isDragging = false;
        currentSplitter = null;
        
        document.body.classList.remove('dragging');
        document.body.classList.remove('dragging-vertical');
        
        // Re-enable pointer events on iframes
        document.querySelectorAll('iframe').forEach(iframe => {
            iframe.style.pointerEvents = 'auto';
        });
        
        // Final repaint after drag ends
        forceBrowserRepaint();
    }

    // Initialize when DOM is ready
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', initSplitters);
    } else {
        initSplitters();
    }
    
    // Send initial viewport dimensions after page loads
    window.addEventListener('load', function() {
        setTimeout(function() {
            notifyViewportResize();
        }, 200);
    });
})();
