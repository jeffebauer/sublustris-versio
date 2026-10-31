// Site-wide settings and links. Everything a non-developer may need to flip is here.

export const REPO = 'jeffebauer/sublustris-versio';
export const REPO_URL = `https://github.com/${REPO}`;

/**
 * The go-public switch (BRIEF §8). Off until the repository is public and the first release
 * is out: the download buttons say "coming soon". DOWNLOADS_LIVE=1 in the environment turns
 * them on (they then link to the latest release's stable-named files).
 */
export const DOWNLOADS_LIVE = process.env.DOWNLOADS_LIVE === '1';

export const DOWNLOAD = {
  firmware: `${REPO_URL}/releases/latest/download/sublustris-versio-firmware.bin`,
  plugin: `${REPO_URL}/releases/latest/download/sublustris-versio-plugin-macos.zip`,
  release: `${REPO_URL}/releases/latest`,
};

export const NAV = [
  { href: '/', label: 'Home' },   // pages only in the nav (owner, 7 Oct: was the #sound anchor)
  { href: '/manual/', label: 'Manual' },
  { href: '/install/', label: 'Install' },
  { href: '/presets/', label: 'Presets' },
  { href: '/changelog/', label: 'Changelog' },
  { href: '/faq/', label: 'FAQ' },
];

export const DOCS = [
  { href: '/manual/', label: 'Manual' },
  { href: '/install/', label: 'Install' },
  { href: '/presets/', label: 'Starting points' },
  { href: '/changelog/', label: 'Changelog' },
  { href: '/faq/', label: 'FAQ' },
  { href: '/credits/', label: 'Credits' },
];

export const LINKS = {
  source: REPO_URL,
  issues: `${REPO_URL}/issues`,
  // The two issue forms (.github/ISSUE_TEMPLATE), opened directly; the FAQ answer has the detail.
  newIssue: `${REPO_URL}/issues/new/choose`,
  soundIdea: `${REPO_URL}/issues/new?template=sound_idea.yml`,
  bugReport: `${REPO_URL}/issues/new?template=bug_report.yml`,
  feedbackFaq: '/faq/#how-do-i-give-feedback-or-report-a-bug',
  mit: `${REPO_URL}/blob/main/LICENSE`,
  agpl: `${REPO_URL}/blob/main/LICENSES/AGPL-3.0.txt`,
  notice: `${REPO_URL}/blob/main/NOTICE`,
  author: 'https://jessebauer.xyz',
  firmwareSwap: 'https://noiseengineering.us/portal/firmware',
};
