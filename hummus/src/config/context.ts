import { createContext } from 'react';

export interface ThemeContext {
  theme: string;
  background: string;
  color: string;
  link: string;
};

export const themes: { [key: string]: ThemeContext } = {
  lightTheme: {
    theme: 'light',
    background: 'white',
    color: 'grey-dark',
    link: 'secondary',
  },
  darkTheme: {
    theme: 'dark',
    background: 'grey-darker',
    color: 'white',
    link: 'white',
  }
};

export const ThemeContext = createContext(themes.darkTheme);
