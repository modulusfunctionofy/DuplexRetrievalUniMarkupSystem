import { root, appendFooter } from '../components/shared.js';
import { fetchPage } from '../api/page.js';
import { login, signup } from '../api/auth.js';
import { navigate } from '../router.js';

export async function renderLogin() {
    root.innerHTML = '<div class="auth-wrapper"><div class="auth-card"><p>Loading...</p></div></div>';

    try {
        const html = await fetchPage('login');
        root.innerHTML = `<div class="auth-wrapper">${html}</div>`;

        const form = document.getElementById('loginForm');
        if (form) {
            form.addEventListener('submit', async (evt) => {
                evt.preventDefault();
                const fd = new FormData(evt.target);
                const success = await login(fd.get('username'), fd.get('password'));
                if (success) {
                    navigate('/dashboard');
                } else {
                    alert('Invalid username or password');
                }
            });
        }

        const gotoSignup = document.getElementById('goto-signup');
        if (gotoSignup) {
            gotoSignup.addEventListener('click', (e) => {
                e.preventDefault();
                navigate('/signup');
            });
        }
    } catch (err) {
        root.innerHTML = '<div class="auth-wrapper"><div class="auth-card"><p>Failed to load login page.</p></div></div>';
        console.error(err);
    }
    appendFooter();
}

export async function renderSignup() {
    root.innerHTML = '<div class="auth-wrapper"><div class="auth-card"><p>Loading...</p></div></div>';

    try {
        const html = await fetchPage('signup');
        root.innerHTML = `<div class="auth-wrapper">${html}</div>`;

        const form = document.getElementById('signupForm');
        if (form) {
            form.addEventListener('submit', async (evt) => {
                evt.preventDefault();
                const fd = new FormData(evt.target);
                const pass = fd.get('password');
                const confirm = fd.get('confirm');

                if (pass !== confirm) {
                    alert("Passwords do not match");
                    return;
                }

                const res = await signup(fd.get('username'), pass);
                if (res.ok) {
                    alert('Account created successfully! Please log in.');
                    navigate('/login');
                } else if (res.status === 409) {
                    alert('Username already exists. Please choose a different one.');
                } else {
                    alert('Failed to create account. Please try again later.');
                }
            });
        }

        const gotoLogin = document.getElementById('goto-login');
        if (gotoLogin) {
            gotoLogin.addEventListener('click', (e) => {
                e.preventDefault();
                navigate('/login');
            });
        }
    } catch (err) {
        root.innerHTML = '<div class="auth-wrapper"><div class="auth-card"><p>Failed to load signup page.</p></div></div>';
        console.error(err);
    }
    appendFooter();
}
