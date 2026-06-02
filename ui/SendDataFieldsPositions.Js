// Sends the bounding rects of all [data-field] elements inside a panel iframe
// to the C++ server so the Vulkan text overlay can be positioned correctly.

function sendFieldPositionsForIframe(iframeSelector) {
    var iframe = document.querySelector(iframeSelector);
    if (!iframe) return;

    try {
        var doc = iframe.contentDocument || iframe.contentWindow.document;
        if (!doc) return;

        var container = doc.querySelector('[data-panel]');
        if (!container) return;

        var panelName = container.getAttribute('data-panel');
        var fields = [];

        container.querySelectorAll('[data-field]').forEach(function(el) {
            var rect = el.getBoundingClientRect();
            if (rect.width > 0 && rect.height > 0) {
                fields.push({
                    field: el.getAttribute('data-field'),
                    x: Math.round(rect.left),
                    y: Math.round(rect.top),
                    w: Math.round(rect.width),
                    h: Math.round(rect.height)
                });
            }
        });

        if (fields.length > 0) {
            fetch('http://localhost:8080/api/dataholder/field-positions', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ panel: panelName, fields: fields })
            }).catch(function() {});
        }
    } catch(e) {}
}

function sendAllPanelFieldPositions() {
    sendFieldPositionsForIframe('.object-inspector-panel iframe');
}

window.sendAllPanelFieldPositions = sendAllPanelFieldPositions;
