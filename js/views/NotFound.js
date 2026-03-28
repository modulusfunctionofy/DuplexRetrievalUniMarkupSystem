import { root, appendFooter } from '../components/shared.js';
import { initCanvas, drawGrid, cleanupCanvas } from '../components/gridBackground.js';
import { navigate } from '../router.js';

export function render404() {
    cleanupCanvas(); // Ensure clean state
    root.innerHTML = `
        <canvas id="grid"></canvas>
        <nav>
            <div class="nav-container">
                <div class="logo">DRUMS</div>
                <div class="nav-buttons">
                    <button id="home-btn" style="background:#52002d; color:#fff; border:none; padding:8px 16px; border-radius:6px; cursor:pointer;">Home</button>
                </div>
            </div>
        </nav>
        <main>
            <div class="hero">
                <h1 style="font-size: 100px; color: #52002d; margin-bottom: 10px;">404</h1>
                <h2>Page Not Found</h2>
                <br>
                <p class="description">
                    Oops! The page you are looking for does not exist in the Duplex Retrieval Uni Markup System.
                </p>
                <button id="return-btn" class="continue-btn" style="margin-top: 30px;">Return Home →</button>
            </div>
        </main>
    `;
    initCanvas();
    drawGrid();

    const goHome = () => {
        navigate('/', true);
    };

    document.getElementById('home-btn').addEventListener('click', goHome);
    document.getElementById('return-btn').addEventListener('click', goHome);

    if (window.location.pathname !== '/404') {
        history.replaceState({ page: '404' }, '', window.location.pathname);
    }
    appendFooter();
}
