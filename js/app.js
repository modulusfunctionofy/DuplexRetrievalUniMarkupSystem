import { handleRoute } from './router.js';

// Setup Popstate listener for back/forward browser navigation
window.addEventListener('popstate', e => {
    handleRoute(window.location.pathname);
});

// Initialize SPA
handleRoute(window.location.pathname);
