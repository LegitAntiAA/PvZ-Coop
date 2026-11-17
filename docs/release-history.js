/* Static GitHub Release history. No client-side GitHub API calls. */
(function () {
  'use strict';

  var REPO = 'https://github.com/LegitAntiAA/PvZ-Coop';
  var mount = null;
  var releases = [];
  var selectedTag = null;
  var loading = true;
  var error = false;
  var copy = {
    en: {
      label: '05 / Release history',
      title: 'EVERY UPDATE.',
      intro: 'Explore the latest release or revisit changes from earlier versions. Notes are copied from GitHub Releases when a release is published.',
      versions: 'Versions',
      latest: 'LATEST',
      published: 'Published',
      download: 'Download ZIP',
      github: 'View on GitHub',
      empty: 'No release notes were provided for this version.',
      loading: 'Loading release history…',
      error: 'Release history is temporarily unavailable. You can still see all versions on GitHub.',
      openAll: 'View all releases',
      unavailable: 'There are no published releases yet.'
    },
    ru: {
      label: '05 / История обновлений',
      title: 'ИСТОРИЯ ВЕРСИЙ.',
      intro: 'Выбирай версию и смотри полный список изменений, включая предыдущие релизы. Данные автоматически сохраняются из GitHub Releases после публикации.',
      versions: 'Версии',
      latest: 'ПОСЛЕДНЯЯ',
      published: 'Опубликовано',
      download: 'Скачать ZIP',
      github: 'Открыть на GitHub',
      empty: 'Для этой версии разработчик не указал список изменений.',
      loading: 'Загружаем историю версий…',
      error: 'История версий временно недоступна. Все релизы можно посмотреть на GitHub.',
      openAll: 'Все релизы',
      unavailable: 'Опубликованных релизов пока нет.'
    }
  };

  function language() {
    return document.documentElement.lang === 'ru' ? 'ru' : 'en';
  }

  function element(tag, className, content) {
    var node = document.createElement(tag);
    if (className) node.className = className;
    if (content !== undefined && content !== null) node.textContent = content;
    return node;
  }

  function link(url, label, className) {
    var node = element('a', className, label);
    node.href = url;
    node.target = '_blank';
    node.rel = 'noopener noreferrer';
    return node;
  }

  function displayVersion(release) {
    var value = String(release.version || release.tag || '');
    return /^\d/.test(value) ? 'v' + value : value;
  }

  function dateLabel(iso) {
    if (!iso) return '';
    var date = new Date(iso);
    if (isNaN(date.getTime())) return '';
    return new Intl.DateTimeFormat(language() === 'ru' ? 'ru-RU' : 'en-US', {
      day: 'numeric', month: 'short', year: 'numeric', timeZone: 'UTC'
    }).format(date);
  }

  function latestRelease() {
    return releases.length ? releases[0] : null;
  }

  // The main website bundle has fallback 0.0.2a links. Replace them with data
  // from the generated JSON; React language switches do not change the data.
  function refreshLatestVersion() {
    var latest = latestRelease();
    if (!latest) return;
    document.querySelectorAll('.hero-copy .version').forEach(function (node) {
      node.textContent = displayVersion(latest);
    });
    document.querySelectorAll('.play-copy .fineprint').forEach(function (node) {
      node.textContent = latest.tag + ' · Windows';
    });
    document.querySelectorAll('.site .hero-ctas a.button-primary, .site .play-copy a.button-primary').forEach(function (node) {
      node.href = latest.download_url || latest.url;
    });
    document.querySelectorAll('.site .quick-facts a[href*="/releases/"], .site .alpha-links a[href*="/releases/"]').forEach(function (node) {
      node.href = latest.url;
    });
    document.title = 'PvZ Co-op ' + displayVersion(latest) + ' — Same lawn. New company.';
  }

  function render() {
    if (!mount) return;
    var t = copy[language()];
    mount.replaceChildren();

    var header = element('div', 'release-history-header');
    var heading = element('div', 'release-history-heading');
    heading.appendChild(element('p', 'section-label', t.label));
    heading.appendChild(element('h2', '', t.title));
    header.appendChild(heading);
    header.appendChild(element('p', 'release-history-intro', t.intro));
    mount.appendChild(header);

    if (loading || error || !releases.length) {
      var message = element('div', 'release-history-message', loading ? t.loading : (error ? t.error : t.unavailable));
      message.setAttribute('role', 'status');
      mount.appendChild(message);
      if (error) mount.appendChild(link(REPO + '/releases', t.openAll, 'release-history-all-link'));
      return;
    }

    var layout = element('div', 'release-history-layout');
    var versions = element('div', 'release-history-versions');
    versions.setAttribute('role', 'tablist');
    versions.setAttribute('aria-label', t.versions);

    releases.forEach(function (release, i) {
      var active = release.tag === selectedTag;
      var button = element('button', 'release-history-version' + (active ? ' active' : ''));
      button.type = 'button';
      button.setAttribute('role', 'tab');
      button.setAttribute('aria-selected', String(active));
      button.setAttribute('aria-controls', 'release-history-panel');
      button.id = 'release-history-tab-' + i;
      button.appendChild(element('span', 'release-history-version-title', displayVersion(release)));
      if (i === 0) button.appendChild(element('span', 'release-history-latest', t.latest));
      button.appendChild(element('small', 'release-history-version-date', dateLabel(release.published_at)));
      button.addEventListener('click', function () {
        selectedTag = release.tag;
        render();
      });
      versions.appendChild(button);
    });

    var current = releases.find(function (release) { return release.tag === selectedTag; }) || releases[0];
    var currentIndex = releases.indexOf(current);
    var panel = element('article', 'release-history-panel');
    panel.id = 'release-history-panel';
    panel.setAttribute('role', 'tabpanel');
    panel.setAttribute('aria-labelledby', 'release-history-tab-' + currentIndex);
    panel.setAttribute('tabindex', '0');

    var detail = element('div', 'release-history-detail');
    detail.appendChild(element('p', 'release-history-date-label', t.published + ' · ' + dateLabel(current.published_at)));
    detail.appendChild(element('h3', '', displayVersion(current)));
    detail.appendChild(element('p', 'release-history-release-title', current.title));

    var actions = element('div', 'release-history-actions');
    if (current.download_url) actions.appendChild(link(current.download_url, t.download, 'release-history-download'));
    actions.appendChild(link(current.url, t.github, 'release-history-github'));
    detail.appendChild(actions);
    panel.appendChild(detail);

    var notes = element('div', 'release-history-markdown');
    var rendered = current.notes && (current.notes[language()] || current.notes.en || current.notes.ru);
    if (rendered) {
      // HTML is generated by GitHub's sanitized Markdown endpoint and stored
      // as a same-origin static file, never fetched from visitors' API tokens.
      notes.innerHTML = rendered;
      notes.querySelectorAll('a').forEach(function (node) {
        node.rel = 'noopener noreferrer';
        if (node.hostname !== window.location.hostname) node.target = '_blank';
      });
    } else {
      notes.appendChild(element('p', '', t.empty));
    }
    panel.appendChild(notes);
    layout.appendChild(versions);
    layout.appendChild(panel);
    mount.appendChild(layout);
  }

  function load() {
    fetch('./releases.json', { cache: 'no-cache' })
      .then(function (response) {
        if (!response.ok) throw new Error('releases.json HTTP ' + response.status);
        return response.json();
      })
      .then(function (data) {
        if (!data || data.schema !== 1 || !Array.isArray(data.releases)) {
          throw new Error('Unexpected releases.json format');
        }
        releases = data.releases.filter(function (entry) {
          return entry && typeof entry.tag === 'string' && typeof entry.url === 'string';
        });
        selectedTag = releases.length ? releases[0].tag : null;
        loading = false;
        render();
        refreshLatestVersion();
      })
      .catch(function (reason) {
        console.error('PvZ-Coop: unable to load release history', reason);
        loading = false;
        error = true;
        render();
      });
  }

  function initialize() {
    if (mount && mount.isConnected) return true;
    mount = document.getElementById('changelog');
    if (!mount) return false;
    render();
    new MutationObserver(function () {
      render();
      refreshLatestVersion();
    }).observe(document.documentElement, { attributes: true, attributeFilter: ['lang'] });
    load();
    return true;
  }

  if (!initialize()) {
    var watcher = new MutationObserver(function () {
      if (initialize()) watcher.disconnect();
    });
    watcher.observe(document.documentElement, { childList: true, subtree: true });
    document.addEventListener('DOMContentLoaded', function () {
      if (initialize()) watcher.disconnect();
    }, { once: true });
  }
})();
