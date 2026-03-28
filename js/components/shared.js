export const root = document.getElementById('root');

export async function appendFooter() {
    const footerContainer = document.getElementById('footer-container') || document.createElement('div');
    footerContainer.id = 'footer-container';
    try {
        const response = await fetch('/footer.html');
        if (response.ok) {
            footerContainer.innerHTML = await response.text();
            if (!document.getElementById('footer-container')) {
                document.body.appendChild(footerContainer);
            }
        }
    } catch (err) {
        console.error('Failed to load footer:', err);
    }
}
