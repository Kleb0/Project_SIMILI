const slotsContainer = document.getElementById('slots-container');
const TOTAL_SLOTS = 50;
let selectedSlots = new Set();
let lastObjectCount = 0;
let slotsCreated = false;
let pendingSelection = false;

async function fetchSceneObjects() {
    const response = await fetch('http://localhost:8080/api/scene/objects');
    
    if (!response.ok) {
        return;
    }
    
    const objects = await response.json();
    
    // Create slots only once at initialization
    if (!slotsCreated) {
        createEmptySlots();
        slotsCreated = true;
    }
    
    // Update slot content and selection state
    updateSlots(objects);
}

function createEmptySlots() {
    slotsContainer.innerHTML = '';
    
    for (let i = 0; i < TOTAL_SLOTS; i++) {
        const slot = document.createElement('div');
        slot.className = 'slot';
        slot.dataset.slotIndex = i;
        slot.textContent = `| --- [slot ${i + 1}] --- |`;
        slotsContainer.appendChild(slot);
    }
}

function updateSlots(objects) {
    const slots = slotsContainer.children;
    
    for (let i = 0; i < TOTAL_SLOTS; i++) {
        const slot = slots[i];
        
        if (i < objects.length) {
            const obj = objects[i];
            
            // Update slot content only if it changed
            const expectedText = `[${i}]--- [${obj.name}] ---`;
            if (slot.textContent !== expectedText) {
                slot.textContent = expectedText;
                slot.dataset.objectId = obj.id;
                slot.dataset.objectName = obj.name;
                slot.dataset.objectType = obj.type;
                slot.title = `Type: ${obj.type}\nID: ${obj.id}`;
                
                // Remove old listener and add new one
                const newSlot = slot.cloneNode(true);
                newSlot.addEventListener('click', (e) => {
                    e.preventDefault();
                    selectObject(i, obj, e);
                });
                newSlot.addEventListener('mousedown', (e) => {
                    if (e.shiftKey) {
                        e.preventDefault();
                    }
                    enableSlotTextureRendering(true);
                });
                slot.parentNode.replaceChild(newSlot, slot);
            }
            
            // Update selection state ONLY if no pending local selection
            if (!pendingSelection) {
                if (obj.selected && !slots[i].classList.contains('selected')) {
                    slots[i].classList.add('selected');
                    selectedSlots.add(i);
                } else if (!obj.selected && slots[i].classList.contains('selected')) {
                    slots[i].classList.remove('selected');
                    selectedSlots.delete(i);
                }
            }
        } else {
            // Reset to empty slot
            const expectedText = `| --- [slot ${i + 1}] --- |`;
            if (slot.textContent !== expectedText) {
                slot.textContent = expectedText;
                delete slot.dataset.objectId;
                delete slot.dataset.objectName;
                delete slot.dataset.objectType;
                slot.title = '';
                slot.classList.remove('selected');
                
                // Remove click listener
                const newSlot = slot.cloneNode(true);
                slot.parentNode.replaceChild(newSlot, slot);
            }
        }
    }
}

function updateSelectionState(objects) {
    const slots = slotsContainer.children;
    
    for (let i = 0; i < objects.length && i < slots.length; i++) {
        const obj = objects[i];
        const slot = slots[i];
        
        if (obj.selected && !slot.classList.contains('selected')) {
            slot.classList.add('selected');
            selectedSlots.add(i);
        } else if (!obj.selected && slot.classList.contains('selected')) {
            slot.classList.remove('selected');
            selectedSlots.delete(i);
        }
    }
}

function displayObjects(objects) {
    slotsContainer.innerHTML = ''; 
    
    for (let i = 0; i < TOTAL_SLOTS; i++) {
        const slot = document.createElement('div');
        slot.className = 'slot';
        slot.dataset.slotIndex = i;
        
        if (i < objects.length) {
            const obj = objects[i];
            slot.textContent = `[${i}]--- [${obj.name}] ---`;
            slot.dataset.objectId = obj.id;
            slot.dataset.objectName = obj.name;
            slot.dataset.objectType = obj.type;
            
            slot.title = `Type: ${obj.type}\nID: ${obj.id}`;
            
            if (obj.selected) {
                slot.classList.add('selected');
                selectedSlots.add(i);
            }
            
            slot.addEventListener('click', (e) => {
                e.preventDefault();
                selectObject(i, obj, e);
            });
            slot.addEventListener('mousedown', (e) => {
                if (e.shiftKey) {
                    e.preventDefault();
                }
                enableSlotTextureRendering(true);
            });
        } else {
            slot.textContent = `| --- [slot ${i + 1}] --- |`;
        }
        
        slotsContainer.appendChild(slot);
    }
}

async function selectObject(slotIndex, obj, event) {
    // Block server updates during local selection
    pendingSelection = true;
    
    if (obj.type !== 'Camera') {
        const clickedSlot = slotsContainer.children[slotIndex];
        
        // If Shift is NOT pressed, clear all previous selections
        if (!event.shiftKey) {
            document.querySelectorAll('.slot.selected').forEach(s => {
                s.classList.remove('selected');
            });
            selectedSlots.clear();
        }
        
        // Add/toggle the clicked slot
        if (clickedSlot) {
            if (event.shiftKey && clickedSlot.classList.contains('selected')) {
                // Shift+Click on already selected slot: deselect it
                clickedSlot.classList.remove('selected');
                selectedSlots.delete(slotIndex);
            } else {
                // Normal click or Shift+Click on unselected: select it
                clickedSlot.classList.add('selected');
                selectedSlots.add(slotIndex);
            }
        }
    }
    
    const response = await fetch('http://localhost:8080/api/select-object', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({
            slotIndex: slotIndex,
            objectId: obj.id,
            objectName: obj.name,
            objectType: obj.type,
            shiftKey: event.shiftKey
        })
    });
    
    // Allow server updates again after 200ms
    setTimeout(() => {
        pendingSelection = false;
    }, 200);
}

async function enableSlotTextureRendering(enable) {
    try {
        await fetch('http://localhost:8080/api/slot-texture/render', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ enable: enable })
        });
    } catch (error) {
    }
}

fetchSceneObjects();

setInterval(() => {
    fetchSceneObjects();
}, 200);
document.addEventListener('mouseup', () => {
    enableSlotTextureRendering(false);
});

