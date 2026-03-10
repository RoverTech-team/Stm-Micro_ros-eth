// assets/js/docs-enhancements.js

document.addEventListener('DOMContentLoaded', () => {
  // 1. Callout replacement
  document.querySelectorAll('.main-content blockquote p').forEach(p => {
    const map = {
      '[!TIP]':     { cls: 'callout-tip',     icon: '💡', label: 'Tip' },
      '[!NOTE]':    { cls: 'callout-note',    icon: 'ℹ️', label: 'Note' },
      '[!CAUTION]': { cls: 'callout-caution', icon: '⚠️', label: 'Caution' },
      '[!WARNING]': { cls: 'callout-warning', icon: '🚫', label: 'Warning' },
      '[!IMPORTANT]': { cls: 'callout-important', icon: '📢', label: 'Important' },
    };
    for (const [token, config] of Object.entries(map)) {
      if (p.textContent.trim().startsWith(token)) {
        const bq = p.closest('blockquote');
        bq.classList.add('callout', config.cls);
        p.innerHTML = p.innerHTML.replace(
          token,
          `<strong class="callout-label">${config.icon} ${config.label}:</strong>`
        );
      }
    }
  });

  // 2. Heading anchors (if not enabled via config)
  document.querySelectorAll('.main-content h2, .main-content h3, .main-content h4').forEach(h => {
    if (!h.id || h.querySelector('.heading-anchor')) return;
    const a = document.createElement('a');
    a.href = '#' + h.id;
    a.className = 'heading-anchor';
    a.innerHTML = ' <span aria-hidden="true">#</span>';
    h.appendChild(a);
  });

  // 3. Glossary Filter
  const filterInput = document.getElementById('glossary-filter');
  if (filterInput) {
    filterInput.addEventListener('input', function() {
      const q = this.value.toLowerCase();
      document.querySelectorAll('.main-content table tbody tr').forEach(row => {
        row.style.display = row.textContent.toLowerCase().includes(q) ? '' : 'none';
      });
    });
  }
});
