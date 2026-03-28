import { renderHome } from './views/Home.js';
import { renderLogin, renderSignup } from './views/AuthViews.js';
import { renderDashboard, cleanupBoard } from './views/Dashboard.js';
import { render404 } from './views/NotFound.js';

export function navigate(path, replace = false) {
    if (window.location.pathname !== path) {
        if (replace) {
            history.replaceState({ page: path.substring(1) || 'home' }, '', path);
        } else {
            history.pushState({ page: path.substring(1) || 'home' }, '', path);
        }
    }
    handleRoute(path);
}

export function handleRoute(path) {
    // If navigating away from dashboard, cleanup board listeners
    if (path !== '/dashboard') {
        if (typeof cleanupBoard === 'function') cleanupBoard();
    }
    switch (path) {
        case '/': renderHome(); break;
        case '/login': renderLogin(); break;
        case '/signup': renderSignup(); break;
        case '/dashboard': renderDashboard(); break;
        case '/404': render404(); break;
        default: render404();
    }
}
