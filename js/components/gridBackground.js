let canvas, ctx;
let width, height;
let mouse = { x: -9999, y: -9999 };
const squareSize = 40;
const grid = [];
let animationFrameId = null;

export function initCanvas() {
    canvas = document.getElementById('grid');
    if (!canvas) return;
    ctx = canvas.getContext('2d');
    width = canvas.width = window.innerWidth;
    height = canvas.height = window.innerHeight;
    initGrid();

    // Prevent duplicate listeners
    window.removeEventListener('resize', handleResize);
    window.removeEventListener('mousemove', handleMouseMove);
    window.addEventListener('resize', handleResize);
    window.addEventListener('mousemove', handleMouseMove);
}

export function cleanupCanvas() {
    window.removeEventListener('resize', handleResize);
    window.removeEventListener('mousemove', handleMouseMove);
    if (animationFrameId) cancelAnimationFrame(animationFrameId);
}

function handleResize() {
    if (!canvas) return;
    width = canvas.width = window.innerWidth;
    height = canvas.height = window.innerHeight;
    initGrid();
}

function handleMouseMove(e) {
    mouse.x = e.clientX;
    mouse.y = e.clientY;
    const cell = getCellAt(mouse.x, mouse.y);
    if (cell && cell.alpha === 0) {
        cell.alpha = 1;
        cell.lastTouched = Date.now();
        cell.fading = false;
    }
}

function initGrid() {
    grid.length = 0;
    for (let x = 0; x < width; x += squareSize) {
        for (let y = 0; y < height; y += squareSize) {
            grid.push({
                x: x,
                y: y,
                alpha: 0,
                fading: false,
                lastTouched: 0
            });
        }
    }
}

function getCellAt(x, y) {
    return grid.find(cell =>
        x >= cell.x && x < cell.x + squareSize &&
        y >= cell.y && y < cell.y + squareSize
    );
}

export function drawGrid() {
    if (!ctx) return;
    ctx.clearRect(0, 0, width, height);
    const now = Date.now();

    for (let cell of grid) {
        if (cell.alpha > 0 && !cell.fading && now - cell.lastTouched > 500) {
            cell.fading = true;
        }
        if (cell.fading) {
            cell.alpha -= 0.025;
            if (cell.alpha <= 0) {
                cell.alpha = 0;
                cell.fading = false;
            }
        }
        if (cell.alpha > 0) {
            const centerX = cell.x + squareSize / 2;
            const centerY = cell.y + squareSize / 2;
            const gradient = ctx.createRadialGradient(
                centerX, centerY, 6,
                centerX, centerY, squareSize * .9
            );
            gradient.addColorStop(0, `rgba(0, 0, 0, ${cell.alpha})`);
            gradient.addColorStop(1, `rgb(85, 3, 30)`);

            ctx.strokeStyle = gradient;
            ctx.lineWidth = 0.4;
            ctx.strokeRect(cell.x + 0.5, cell.y + 0.5, squareSize - 1, squareSize - 1);
        }
    }
    animationFrameId = requestAnimationFrame(drawGrid);
}
