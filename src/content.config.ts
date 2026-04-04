import { defineCollection, z } from 'astro:content';
import { glob } from 'astro/loaders';

const news = defineCollection({
  loader: glob({ pattern: '**/*.md', base: './src/content/news' }),
  schema: z.object({
    title: z.string(),
    date: z.string(),
    tag: z.enum(['update', 'feature', 'release']),
    summary: z.string(),
  }),
});

export const collections = { news };
