import React from 'react';
import type {ReactNode} from 'react';
import MDXComponents from '@theme-original/MDXComponents';
import Admonition from '@theme/Admonition';

// Контент постепенно переезжает на Mintlify-синтаксис (<Warning>/<Note>
// вместо GitHub-alert блоков) - маппим его на существующий Admonition, чтобы
// сборка здесь не ломалась, пока Mintlify не подтверждён живьём и этот сайт
// остаётся рабочим фолбэком.
export default {
  ...MDXComponents,
  Warning: (props: {children: ReactNode}) => (
    <Admonition type="warning">{props.children}</Admonition>
  ),
  Note: (props: {children: ReactNode}) => (
    <Admonition type="note">{props.children}</Admonition>
  ),
};
