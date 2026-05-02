import { root, appendFooter } from '../components/shared.js';
import { fetchPage } from '../api/page.js';
import { login, signup } from '../api/auth.js';
import { navigate } from '../router.js';

export async function renderLogin() {
    root.innerHTML = `
        <div class="auth-wrapper">
            <div class="auth-card">
                <p>Loading...</p>
            </div>
        </div>
    `;

    try {
        const html = await fetchPage('login');

        root.innerHTML = `
            <div class="auth-wrapper">
                ${html}
            </div>
        `;

        const form = root.querySelector('#loginForm');

        if (form) {
            form.addEventListener('submit', async (evt) => {
                evt.preventDefault();

                try {
                    const fd = new FormData(form);

                    const username = fd.get('username');
                    const password = fd.get('password');

                    const success = await login(username, password);

                    if (success) {
                        navigate('/dashboard', true);
                    } else {
                        alert('Invalid username or password');
                    }

                } catch (err) {
                    console.error(err);
                    alert('Login request failed');
                }
            });
        }

        const gotoSignup = root.querySelector('#goto-signup');

        if (gotoSignup) {
            gotoSignup.addEventListener('click', (e) => {
                e.preventDefault();
                navigate('/signup');
            });
        }

    } catch (err) {
        console.error(err);

        root.innerHTML = `
            <div class="auth-wrapper">
                <div class="auth-card">
                    <p>Failed to load login page.</p>
                </div>
            </div>
        `;
    }

    appendFooter();
}

export async function renderSignup() {
    root.innerHTML = `
        <div class="auth-wrapper">
            <div class="auth-card">
                <p>Loading...</p>
            </div>
        </div>
    `;

    try {
        const html = await fetchPage('signup');

        root.innerHTML = `
            <div class="auth-wrapper">
                ${html}
            </div>
        `;

        const form = root.querySelector('#signupForm');

        if (form) {
            form.addEventListener('submit', async (evt) => {
                evt.preventDefault();

                try {
                    const fd = new FormData(form);

                    const username = fd.get('username');
                    const password = fd.get('password');
                    const confirm = fd.get('confirm');

                    if (password !== confirm) {
                        alert('Passwords do not match');
                        return;
                    }

                    const result = await signup(username, password);

                    if (result.ok) {
                        alert('Account created successfully! Please log in.');
                        navigate('/login', true);

                    } else if (result.status === 409) {
                        alert('Username already exists. Please choose another one.');

                    } else {
                        alert('Failed to create account.');
                    }

                } catch (err) {
                    console.error(err);
                    alert('Signup request failed');
                }
            });
        }

        const gotoLogin = root.querySelector('#goto-login');

        if (gotoLogin) {
            gotoLogin.addEventListener('click', (e) => {
                e.preventDefault();
                navigate('/login');
            });
        }

    } catch (err) {
        console.error(err);

        root.innerHTML = `
            <div class="auth-wrapper">
                <div class="auth-card">
                    <p>Failed to load signup page.</p>
                </div>
            </div>
        `;
    }

    appendFooter();
}
