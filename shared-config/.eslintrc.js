/**
 * WARNING: Are you sure you want to edit this file? This is global configuration shared by all projects.
 * If you are seeing this unexpectedly, keep in mind that projects symlink to this file in shared-config.
 * If you only wish to override something for a single project (please try not to), then you must copy this
 * file in place of the symlink, and remove this message. It is not possible to be clever and try to include
 * the external configuration file, as Docker builds cannot reference files outside their project folder.
 */
module.exports = {
  root: true,
  parser: '@typescript-eslint/parser',
  plugins: ['@typescript-eslint'],
  overrides: [
    {
      files: ['*.ts', '*.tsx'], // Your TypeScript files extension
      parserOptions: {
        project: ['./tsconfig.json'], // Specify it only for TypeScript files
      },
    },
  ],
  extends: ['eslint:recommended', 'plugin:@typescript-eslint/recommended', 'prettier'],
  env: {
    node: true,
  },
  rules: {
    '@typescript-eslint/explicit-module-boundary-types': 'off',
    '@typescript-eslint/no-non-null-assertion': 'off',
    '@typescript-eslint/no-explicit-any': 'off',
    '@typescript-eslint/no-empty-function': 'off',
    '@typescript-eslint/await-thenable': 'error',
    '@typescript-eslint/no-floating-promises': 2,
    'require-await': 2,
    'no-constant-condition': 'off',
    camelcase: 2,
  },
  ignorePatterns: ['node_modules', 'dest*', 'dist', '*.js', '.eslintrc'],
};
