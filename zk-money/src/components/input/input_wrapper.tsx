import styled, { css } from 'styled-components/macro';
import { breakpoints, colours, spacings } from '../../styles';
import { InputTheme } from './input_theme';

const wrapperStyles: { [key in InputTheme]: any } = {
  [InputTheme.WHITE]: css`
    background: ${colours.white};
    box-shadow: 0px 4px 10px rgb(0 0 0 / 10%);
  `,
  [InputTheme.LIGHT]: css`
    background: rgba(255, 255, 255, 0.2);
  `,
};

export const InputRow = styled.div`
  display: flex;
  padding: 0;
  margin: 0 -${spacings.s};

  @media (max-width: ${breakpoints.s}) {
    flex-direction: column;
  }
`;

export const InputCol = styled.div`
  position: relative;
  padding: ${spacings.m} ${spacings.s};
  width: 100%;

  @media (max-width: ${breakpoints.s}) {
    padding: ${spacings.s};
  }
`;

interface InputWrapperProps {
  theme: InputTheme;
}

export const InputWrapper = styled.div<InputWrapperProps>`
  display: flex;
  position: relative;
  align-items: center;
  border-radius: 15px;
  ${({ theme }: InputWrapperProps) => wrapperStyles[theme]}
`;
