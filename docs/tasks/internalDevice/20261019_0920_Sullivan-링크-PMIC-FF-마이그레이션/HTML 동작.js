(() => {
  'use strict';
  const $ = s => document.querySelector(s);
  const $$ = s => [...document.querySelectorAll(s)];
  const tabs = $$('[role="tab"]');
  const panels = $$('.tab-panel');
  const details = $$('.analysis-detail');
  const names = Object.fromEntries(tabs.map(t => [t.dataset.tab, t.querySelector('b').textContent]));
  const tablist = $('[role="tablist"]');
  let active = 'overview';
  let allMode = false;
  let beforeAll = [];
  let printState = null;
  let selectedSearch = null;
  const announce = text => { $('#announce').textContent = text; };
  const setHash = value => {
    if (location.hash !== '#' + value) history.pushState(null, '', '#' + value);
  };

  function setAll(on, scroll = true) {
    if (allMode === on) return;
    allMode = on;
    document.body.classList.toggle('all-mode', on);
    $('#all-toggle').setAttribute('aria-pressed', String(on));
    $('#all-toggle').textContent = on ? '탭으로 보기' : '전체 보기';
    $('#current-tab').textContent = on ? '전체 보기' : names[active];
    if (on) {
      beforeAll = details.map(d => d.open);
      details.forEach(d => { d.open = true; });
      panels.forEach(p => { p.hidden = false; p.setAttribute('role', 'region'); });
      // In continuous reading mode these are section navigation buttons, not hidden-page tabs.
      tablist.removeAttribute('role');
      tablist.removeAttribute('aria-orientation');
      tabs.forEach(t => { t.removeAttribute('role'); t.removeAttribute('aria-selected'); t.tabIndex = 0; });
      announce('모든 기능과 상세 분석을 펼쳤습니다.');
    } else {
      details.forEach((d, i) => { d.open = beforeAll[i] || false; });
      panels.forEach(p => { p.hidden = p.id !== 'panel-' + active; p.setAttribute('role', 'tabpanel'); });
      tablist.setAttribute('role', 'tablist');
      syncOrientation();
      tabs.forEach(t => { t.setAttribute('role', 'tab'); t.setAttribute('aria-selected', String(t.dataset.tab === active)); t.tabIndex = t.dataset.tab === active ? 0 : -1; });
      announce('선택한 기능의 탭 보기로 돌아왔습니다.');
    }
    if (scroll) window.scrollTo({top: 0, behavior: 'instant'});
  }

  function activate(id, {focus = false, hash = true, scroll = true} = {}) {
    if (!(id in names)) return;
    if (allMode) setAll(false, false);
    active = id;
    panels.forEach(p => { p.hidden = p.id !== 'panel-' + id; });
    tabs.forEach(t => {
      const selected = t.dataset.tab === id;
      t.setAttribute('aria-selected', String(selected));
      t.tabIndex = selected ? 0 : -1;
      if (selected && focus) t.focus({preventScroll: true});
    });
    $('#current-tab').textContent = names[id];
    document.title = names[id] + ' · Sound1 마이그레이션 분석';
    if (hash) setHash(id);
    if (scroll) window.scrollTo({top: 0, behavior: 'instant'});
    const selected = tabs.find(t => t.dataset.tab === id);
    if (matchMedia('(max-width:900px)').matches) selected.scrollIntoView({block:'nearest',inline:'nearest',behavior:'instant'});
  }
  function syncOrientation() {
    if (!allMode) tablist.setAttribute('aria-orientation', matchMedia('(max-width:900px)').matches ? 'horizontal' : 'vertical');
  }
  syncOrientation();
  window.addEventListener('resize', syncOrientation);
  tabs.forEach((tab, i) => {
    tab.addEventListener('click', () => activate(tab.dataset.tab));
    tab.addEventListener('keydown', e => {
      if (allMode) return;
      const horizontal = tablist.getAttribute('aria-orientation') === 'horizontal';
      let next = null;
      if (e.key === 'Home') next = 0;
      if (e.key === 'End') next = tabs.length - 1;
      if (e.key === (horizontal ? 'ArrowRight' : 'ArrowDown')) next = (i + 1) % tabs.length;
      if (e.key === (horizontal ? 'ArrowLeft' : 'ArrowUp')) next = (i - 1 + tabs.length) % tabs.length;
      if (next !== null) { e.preventDefault(); activate(tabs[next].dataset.tab, {focus: true}); }
    });
  });
  $$('[data-go]').forEach(b => b.addEventListener('click', () => {
    activate(b.dataset.go);
    $('#panel-' + b.dataset.go).focus({preventScroll:true});
  }));
  $('#all-toggle').addEventListener('click', () => setAll(!allMode));

  function revealTarget(el) {
    const panel = el.closest('.tab-panel');
    if (panel && !allMode) activate(panel.id.replace('panel-', ''), {hash:false, scroll:false});
    else if (panel) active = panel.id.replace('panel-', '');
    for (let d = el.closest('details'); d; d = d.parentElement.closest('details')) d.open = true;
    requestAnimationFrame(() => el.scrollIntoView({block:'start',behavior:'instant'}));
  }
  function followHash() {
    let key;
    try { key = decodeURIComponent(location.hash.slice(1)); } catch { return; }
    if (key in names) { activate(key, {hash:false}); return; }
    const target = key && document.getElementById(key);
    if (target) revealTarget(target);
  }
  document.addEventListener('click', e => {
    const a = e.target.closest('a[href^="#"]');
    if (!a) return;
    const key = decodeURIComponent(a.getAttribute('href').slice(1));
    if (key === 'main') return;
    if (key in names || document.getElementById(key)) {
      e.preventDefault(); setHash(key); followHash();
    }
  });
  window.addEventListener('popstate', followHash);
  window.addEventListener('hashchange', followHash);
  followHash();

  $$('table').forEach(table => { table.dataset.columns = table.querySelector('tr')?.children.length || 0; });
  const searchDialog = $('#search-dialog');
  const searchInput = $('#search-input');
  searchDialog.addEventListener('keydown', e => {
    if (e.key === 'Escape' && !e.isComposing) { e.preventDefault(); searchDialog.close(); }
  });
  const searchEntries = [];
  panels.forEach(panel => {
    const candidates = [...panel.querySelectorAll('.lead h2,.lead>p,.notice,.info-card,.example-card,.chip,.source-content h3,.source-content h4,.source-content p,.source-content li,.source-content tbody tr,.source-content pre,table caption,.gate-grid article,.roadmap li,.verification-levels article,.rd-node h4,.rd-node p,.rd-transitions li,.rd-connections li,.rd-messages li')];
    candidates.forEach(el => {
      if (el.closest('svg')) return;
      const text = el.textContent.replace(/\s+/g, ' ').trim();
      if (text.length < 5) return;
      if (!el.id) el.id = 'find-' + searchEntries.length;
      searchEntries.push({el, text, lower:text.toLocaleLowerCase('ko'), panel:panel.id.replace('panel-','')});
    });
  });
  $('#search-open').addEventListener('click', () => { searchDialog.showModal(); searchInput.focus(); });
  $$('[data-close]').forEach(b => b.addEventListener('click', () => document.getElementById(b.dataset.close).close()));
  document.addEventListener('keydown', e => {
    if (e.key === '/' && !e.ctrlKey && !e.metaKey && !e.altKey && !e.target.closest('input,textarea,select,[contenteditable=true]') && !$$('dialog[open]').length) {
      e.preventDefault(); searchDialog.showModal(); searchInput.focus();
    }
  });
  function resultSnippet(text, term) {
    const idx = text.toLocaleLowerCase('ko').indexOf(term.toLocaleLowerCase('ko'));
    const start = Math.max(0, idx - 42);
    const end = Math.min(text.length, idx + term.length + 80);
    const fragment = document.createDocumentFragment();
    fragment.append((start ? '…' : '') + text.slice(start, idx));
    const mark = document.createElement('mark'); mark.className = 'search-hit'; mark.textContent = text.slice(idx, idx + term.length); fragment.append(mark);
    fragment.append(text.slice(idx + term.length, end) + (end < text.length ? '…' : ''));
    return fragment;
  }
  searchInput.addEventListener('input', () => {
    const term = searchInput.value.trim();
    const results = $('#search-results'); results.replaceChildren();
    if (!term) { $('#search-count').textContent = '검색어를 입력하면 상세 분석까지 찾습니다.'; return; }
    const query = term.toLocaleLowerCase('ko');
    const matches = searchEntries.filter(x => x.lower.includes(query));
    $('#search-count').textContent = matches.length ? `${matches.length}개 결과${matches.length > 30 ? ', 앞의 30개 표시' : ''}` : '검색 결과가 없습니다. 더 짧은 용어나 변수명으로 찾아보세요.';
    matches.slice(0,30).forEach(entry => {
      const button = document.createElement('button'); button.type = 'button'; button.className = 'search-result';
      const small = document.createElement('small'); small.textContent = names[entry.panel];
      const snippet = document.createElement('span'); snippet.append(resultSnippet(entry.text, term));
      button.append(small, snippet);
      button.addEventListener('click', () => {
        searchDialog.close();
        if (selectedSearch) selectedSearch.classList.remove('search-target');
        selectedSearch = entry.el; selectedSearch.classList.add('search-target');
        revealTarget(entry.el); setHash(entry.el.id);
        requestAnimationFrame(() => { entry.el.setAttribute('tabindex', '-1'); entry.el.focus({preventScroll:true}); });
      });
      results.append(button);
    });
  });

  /* RESPONSIVE_DIAGRAM_RUNTIME */

  function computeLedger({P,Pmax,S,C,R,G}) {
    const numbers = [P,Pmax,S,C,R,G];
    if (numbers.some(n => !Number.isInteger(n) || n < 0 || n > 255)) return {error:'0–255 범위의 정수 필요'};
    if (C > S) return {error:'C가 S보다 큼'};
    const request = S-C, budget = Math.min(S,G)-R, voltage = Pmax-P;
    const delta = Math.max(0,Math.min(request,budget,voltage));
    return {request,budget,voltage,delta,Pnext:P+delta,Rnext:R+delta,Cnext:S};
  }
  // Only the arithmetic illustration is exposed for local verification. No hardware/network API exists.
  window.Sound1Report = Object.freeze({computeLedger});
  function updateCalc() {
    const v = Object.fromEntries(['P','Pmax','S','C','R','G'].map(id => [id,$('#calc-'+id).value === '' ? NaN : Number($('#calc-'+id).value)]));
    const r = computeLedger(v);
    const error = $('#calc-error');
    error.hidden = !r.error;
    if (r.error) {
      error.textContent = v.C > v.S ? '이 계산 예시는 S≥C인 새 요청을 가정합니다. C가 S보다 크면 과거 요청 또는 불일치 여부를 먼저 확인해야 합니다.' : '각 값에 0–255 범위의 정수를 입력해 주세요. 이 입력 범위는 설명용이며 보드 설정의 허용 범위가 아닙니다.';
      ['request','budget','voltage','delta'].forEach(key => { $('#out-'+key).textContent = '—'; });
      $('#out-explanation').textContent = '유효한 값을 입력하면 계산 결과를 표시합니다.';
      $('#out-success').textContent = '입력 오류 상태에서는 계산하지 않습니다.';
      return;
    }
    ['request','budget','voltage','delta'].forEach(key => { $('#out-'+key).textContent = r[key]; });
    if (r.delta > 0) {
      const partial = r.delta < r.request;
      $('#out-explanation').textContent = `Δ = max(0, min(${r.request}, ${r.budget}, ${r.voltage})) = ${r.delta}. ` + (partial ? `요구량 ${v.S}을 모두 올리지 못해도 이번 요청의 C는 ${v.S}로 종결합니다. 같은 요청의 나머지를 나중에 자동으로 채우지 않습니다.` : '같은 사건에서 이미 처리한 양을 빼고 남은 차이만 올립니다.');
      $('#out-success').textContent = `P ${v.P} → ${r.Pnext}, R ${v.R} → ${r.Rnext}, C ${v.C} → ${r.Cnext}. P/R은 실제 양수 쓰기가 성공한 뒤 갱신합니다.`;
    } else {
      $('#out-explanation').textContent = `Δ = max(0, min(${r.request}, ${r.budget}, ${r.voltage})) = 0. 같은 요청이 처리됐거나 보상/레벨 여유가 없습니다. 유효한 0적용 요청은 C=S로 종결합니다.`;
      $('#out-success').textContent = `양수 PMIC 쓰기 없음. P ${v.P}, R ${v.R} 유지. C=${v.S}로 종결하며, 뒤에 여유가 생겨도 같은 요청을 재충전하지 않습니다.`;
    }
  }
  const presets = {
    first:{P:180,Pmax:214,S:12,C:0,R:0,G:20},
    next:{P:192,Pmax:214,S:20,C:12,R:12,G:20},
    cap:{P:210,Pmax:214,S:12,C:0,R:0,G:20},
    repeat:{P:200,Pmax:214,S:20,C:20,R:20,G:20},
    reclaim:{P:199,Pmax:214,S:20,C:20,R:19,G:20}
  };
  $$('[data-preset]').forEach(button => {
    button.setAttribute('aria-pressed', String(button.dataset.preset === 'first'));
    button.addEventListener('click', () => {
      for (const [key,value] of Object.entries(presets[button.dataset.preset])) $('#calc-'+key).value = value;
      $$('[data-preset]').forEach(b => b.setAttribute('aria-pressed', String(b === button)));
      updateCalc();
    });
  });
  $$('.calc-inputs input').forEach(input => input.addEventListener('input', () => { $$('[data-preset]').forEach(b=>b.setAttribute('aria-pressed','false')); updateCalc(); }));
  updateCalc();

  window.addEventListener('beforeprint', () => {
    printState = details.map(d => d.open);
    details.forEach(d => { d.open = true; });
  });
  window.addEventListener('afterprint', () => {
    if (printState) details.forEach((d,i) => { d.open = printState[i]; });
    printState = null;
  });
  $('#print-all').addEventListener('click', () => window.print());
})();
