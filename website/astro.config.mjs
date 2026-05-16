import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';

export default defineConfig({
  site: 'https://balazsbencs.github.io',
  base: '/daisy-multifx-pedal',
  integrations: [
    starlight({
      title: 'Multi-FX',
      description: 'A stereo-out stompbox built on Daisy Seed — Modulation, Delay, and Reverb in series.',
      customCss: ['./src/styles/custom.css'],
      social: [
        {
          icon: 'github',
          label: 'GitHub',
          href: 'https://github.com/balazsbencs/daisy-multifx-pedal',
        },
      ],
      sidebar: [
        {
          label: 'User Manual',
          items: [
            { label: 'Introduction',       slug: 'user-manual/introduction' },
            { label: 'Front Panel',        slug: 'user-manual/front-panel' },
            { label: 'Signal Chain',       slug: 'user-manual/signal-chain' },
            { label: 'Effect Modes',       slug: 'user-manual/effect-modes' },
            { label: 'Parameters',         slug: 'user-manual/parameters' },
            { label: 'Tap & Hold',         slug: 'user-manual/tap-and-hold' },
            { label: 'MIDI',               slug: 'user-manual/midi' },
            { label: 'Presets',            slug: 'user-manual/presets' },
            { label: 'Troubleshooting',    slug: 'user-manual/troubleshooting' },
          ],
        },
        {
          label: 'Developer',
          items: [
            { label: 'Architecture',         slug: 'developer/architecture' },
            { label: 'Signal Path',          slug: 'developer/signal-path' },
            { label: 'Mode System',          slug: 'developer/mode-system' },
            { label: 'DSP Blocks',           slug: 'developer/dsp-blocks' },
            { label: 'Parameters',           slug: 'developer/parameters' },
            { label: 'Display',              slug: 'developer/display' },
            { label: 'MIDI & Tempo',         slug: 'developer/midi-dev' },
            { label: 'Building & Flashing',  slug: 'developer/building' },
          ],
        },
        {
          label: 'Hardware',
          items: [
            { label: 'Pin Map',              slug: 'hardware/pin-map' },
            { label: 'Wiring',               slug: 'hardware/wiring' },
            { label: 'Bill of Materials',    slug: 'hardware/bom' },
          ],
        },
      ],
    }),
  ],
});
