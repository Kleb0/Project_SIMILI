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
        
        // Debounce resize events
        if (resizeTimeout) {
            clearTimeout(resizeTimeout);
        }
        
        resizeTimeout = setTimeout(() => {
            // Reset all sections to use default flex
            const sections = document.querySelectorAll('.left-section, .center-section, .right-section');
            sections.forEach(section => {
                section.style.width = '';
                section.style.flex = '';
            });
            
            const panels = document.querySelectorAll('.panel');
            panels.forEach(panel => {
                panel.style.height = '';
                panel.style.flex = '';
            });
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
            
            // Set minimum heights (100px)
            if (newPrevHeight < 100) {
                const diff = 100 - newPrevHeight;
                newPrevHeight = 100;
                newNextHeight -= diff;
            }
            if (newNextHeight < 100) {
                const diff = 100 - newNextHeight;
                newNextHeight = 100;
                newPrevHeight -= diff;
            }
            
            if (newPrevHeight >= 100 && newNextHeight >= 100) {
                // Use fixed pixel heights for precise control
                prevElement.style.height = newPrevHeight + 'px';
                prevElement.style.flex = '0 0 auto';
                nextElement.style.height = newNextHeight + 'px';
                nextElement.style.flex = '0 0 auto';
                
                // Force browser repaint
                forceBrowserRepaint();
            }
        }
    }
    
    function forceBrowserRepaint() {
        // Notify C++ to force CEF repaint via IPC
        try {
            if (window.chrome && window.chrome.cefQuery) {
                window.chrome.cefQuery({
                    request: 'force_repaint',
                    persistent: false,
                    onSuccess: function(response) {},
                    onFailure: function(error_code, error_message) {}
                });
            }
        } catch (e) {
            // Fallback if CEF query not available
        }
        
        // Force reflow by accessing offsetHeight
        document.body.offsetHeight;
        
        // Trigger paint
        if (window.requestAnimationFrame) {
            window.requestAnimationFrame(() => {
                // Force another reflow
                document.documentElement.scrollTop;
            });
        }
    }

    function handleMouseUp(e) {
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
})();
