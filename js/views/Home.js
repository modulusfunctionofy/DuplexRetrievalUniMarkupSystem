import { root, appendFooter } from '../components/shared.js';
import { initCanvas, drawGrid, cleanupCanvas } from '../components/gridBackground.js';
import { navigate } from '../router.js';

export function renderHome() {
    cleanupCanvas(); // Ensure no duplicates
    root.innerHTML = `
        <canvas id="grid"></canvas>
        <nav>
            <div class="nav-container">
                <div class="logo">DRUMS</div>
                <div class="nav-buttons">
                    <button id="login-btn">Login</button>
                    <button id="signup-btn">Sign Up</button>
                </div>
            </div>
        </nav>
        <main>
            <div class="hero">
                <h1>Assignment Evaluation System using NLP (Duplex Retrieval Uni Markup System)</h1>
                <div class="description" style="text-align: left;">
    <p><strong>Problem:</strong><br>
    &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;In the current education system, the evaluation of answer sheets is done manually, which is a time-consuming and tedious process. Also, the evaluation of subjective papers can be biased and inconsistent.</p>

    <p><strong>Primary Beneficiary:</strong><br>
    &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Centralized Evaluation Camp (The system which evaluates answer sheets at a mass level for subjective papers.)</p>

    <p><strong>Secondary Beneficiary:</strong><br>
    &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Faculties, teachers, and students.</p>

    <p><strong>Solution:</strong></p>
    <ul style="text-align: left;">
        <li>Converting answer sheets into a machine-readable format</li>
        <li>Using machine learning and mathematical algorithms to evaluate answers</li>
        <li>Retrieving live suggestions on top of the answer sheet images</li>
    </ul>

    <p><strong>Introduction of Duplex Retrieval Uni Markup System:</strong></p>
    <ul style="text-align: left;">
        <li><strong>Duplex:</strong> Two-sided comparison between key and responses</li>
        <li><strong>Retrieval:</strong> Retrieving data for and from the answer sheet database</li>
        <li><strong>Uni:</strong> A unified, streamlined flow of information</li>
        <li><strong>Markup:</strong> Annotating and suggesting scores directly on the answer sheet</li>
        <li><strong>System:</strong> Workflow system</li>
    </ul>

    <p><strong>Changes / Impact:</strong></p>
    <ul style="text-align: left;">
        <li>Converting traditional one-dimensional evaluation into multi-dimensional evaluation</li>
        <li>Improving accuracy and fairness</li>
        <li>Reducing time and cost</li>
        <li>Instantly checking factual correctness, response relevance, and structure</li>
    </ul>
</div>
                <button id="continue-btn" class="continue-btn">Continue →</button>
            </div>
        </main>
    `;
    initCanvas();
    drawGrid();

    document.getElementById('continue-btn').addEventListener('click', () => navigate('/dashboard'));
    document.getElementById('login-btn').addEventListener('click', () => navigate('/login'));
    document.getElementById('signup-btn').addEventListener('click', () => navigate('/signup'));

    appendFooter();
}
