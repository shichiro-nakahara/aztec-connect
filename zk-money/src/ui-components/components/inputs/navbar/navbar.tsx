import { Link, useLocation } from 'react-router-dom';
import { bindStyle } from '../../../util/classnames';
import { ReactComponent as Logo } from '../../../images/zk_money.svg';
import style from './navbar.module.scss';
import { useTotalValuation } from 'alt-model/total_account_valuation_hooks';
import { formatBulkPrice } from 'app';

const cx = bindStyle(style);

export enum Theme {
  GRADIENT = 'GRADIENT',
  WHITE = 'WHITE',
}

interface NavbarProps {
  isLoggedIn: boolean;
  path?: string;
  theme?: Theme;
  onLogin?: () => void;
  onChange?: (path: string) => void;
  // NOTE: This is receiving an AccountComponent
  //     this should instead receive an AccountState and
  //     render the AccountComponent within here
  //     this will happen once we migrate AccountComponent to Storybook
  accountComponent?: JSX.Element;
}

interface LinkItem {
  url: string;
  label: string;
  disabled?: boolean;
}

const LINKS: LinkItem[] = [
  { url: '/earn', label: 'Earn' },
  { url: '/trade', label: 'Trade' },
];

export function Navbar({ isLoggedIn, accountComponent, theme, onChange, onLogin }: NavbarProps): JSX.Element {
  const location = useLocation();
  const totalValuation = useTotalValuation();
  const totalValuationStr = totalValuation !== undefined ? formatBulkPrice(totalValuation) : 'Balance';

  return (
    <div className={style.headerRoot}>
      <div className={cx(style.logoRoot, { enabled: !!onChange })}>
        <Link to="/">
          <Logo className={cx(style.logo, theme === Theme.GRADIENT ? style.gradient : style.white)} />
        </Link>
      </div>

      <div className={style.accountRoot}>
        <div />
        {LINKS.map(link => (
          <Link
            key={link.url}
            to={link.url}
            className={cx(style.link, style.navLink, {
              active: link.url === location.pathname,
              white: theme === Theme.WHITE,
              gradient: theme === Theme.GRADIENT,
            })}
          >
            {link.label}
          </Link>
        ))}
        {isLoggedIn ? (
          <div className={style.accountWrapper}>
            <Link
              to={'/balance'}
              className={cx(style.link, style.navLink, {
                active: '/balance' === location.pathname,
                white: theme === Theme.WHITE,
                gradient: theme === Theme.GRADIENT,
              })}
            >
              Wallet
            </Link>
            <div className={style.accountComponent}>{accountComponent}</div>
          </div>
        ) : (
          <Link
            to="/signin"
            className={cx(style.link, style.navLink, {
              active: '/signin' === location.pathname || '/signup' === location.pathname,
              white: theme === Theme.WHITE,
              gradient: theme === Theme.GRADIENT,
            })}
          >
            Log In
          </Link>
        )}
      </div>
    </div>
  );
}
