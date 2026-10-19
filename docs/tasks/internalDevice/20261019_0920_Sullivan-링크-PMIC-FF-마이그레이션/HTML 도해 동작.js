  // Only the connectors are SVG. Nodes, labels, and branch destinations remain reflowing HTML.
  let diagramFrame = 0;
  function drawDiagramConnections() {
    diagramFrame = 0;
    document.querySelectorAll('.rd-graph').forEach(graph => {
      if (!graph.offsetWidth || !graph.offsetHeight) return;
      const box = graph.getBoundingClientRect();
      const cards = [...graph.querySelectorAll('.rd-main > .rd-node')];
      const svg = graph.querySelector('.rd-wires');
      const marker = 'rd-arrow-' + graph.dataset.graph;
      const lines = [];
      for (let i = 0; i + 1 < cards.length; i++) {
        const a = cards[i].getBoundingClientRect();
        const b = cards[i + 1].getBoundingClientRect();
        let d;
        if (Math.abs(a.top - b.top) < 2) {
          const x1 = a.right-box.left+1, x2 = b.left-box.left-3;
          const y = a.top-box.top+a.height/2;
          d = `M ${x1} ${y} L ${x2} ${y}`;
        } else {
          const x1 = a.left-box.left+a.width/2, x2 = b.left-box.left+b.width/2;
          const y1 = a.bottom-box.top+1, y2 = b.top-box.top-3;
          const middle = (y1+y2)/2;
          d = `M ${x1} ${y1} L ${x1} ${middle} L ${x2} ${middle} L ${x2} ${y2}`;
        }
        lines.push(`<path d="${d}" marker-end="url(#${marker})"/>`);
      }
      svg.setAttribute('viewBox', `0 0 ${box.width} ${box.height}`);
      svg.setAttribute('preserveAspectRatio','none');
      svg.setAttribute('width', String(box.width));
      svg.setAttribute('height', String(box.height));
      svg.innerHTML = `<defs><marker id="${marker}" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M 0 0 L 10 5 L 0 10 z"/></marker></defs>${lines.join('')}`;
    });
  }
  function scheduleDiagramConnections() {
    if (!diagramFrame) diagramFrame = requestAnimationFrame(drawDiagramConnections);
  }
  const diagramObserver = new ResizeObserver(scheduleDiagramConnections);
  document.querySelectorAll('.rd-graph').forEach(graph => diagramObserver.observe(graph));
  window.addEventListener('resize', scheduleDiagramConnections);
  window.addEventListener('beforeprint', scheduleDiagramConnections);
  document.fonts.ready.then(scheduleDiagramConnections);
  scheduleDiagramConnections();
  document.addEventListener('click', event => {
    const link = event.target.closest('a[href^="#rd-"]');
    if (!link) return;
    const target = document.getElementById(link.getAttribute('href').slice(1));
    if (target) requestAnimationFrame(() => {
      target.setAttribute('tabindex','-1');
      target.focus({preventScroll:true});
    });
  });
