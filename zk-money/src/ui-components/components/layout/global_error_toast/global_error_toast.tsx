import { useEffect, useState } from 'react';
import { CloseMiniButton } from 'ui-components/components/inputs';
import { bindStyle } from 'ui-components/util/classnames';
import style from './global_error_toast.module.scss';

const cx = bindStyle(style);

const AUTO_HIDE_DELAY = 5000;

export function ErrorToast() {
  const [message, setMessage] = useState('');
  const [hidden, setHidden] = useState(true);
  useEffect(() => {
    const handleRejection = (event: PromiseRejectionEvent) => {
      // This event can fire mid-render, hence why setting state must be deferred
      setTimeout(() => {
        setMessage(event.reason?.toString());
        setHidden(false);
      }, 0);
    };
    const handleError = (event: ErrorEvent) => {
      // This event can fire mid-render, hence why setting state must be deferred
      setTimeout(() => {
        setMessage(event.message?.toString());
        setHidden(false);
      }, 0);
    };
    window.addEventListener('unhandledrejection', handleRejection);
    window.addEventListener('error', handleError);
    return () => {
      window.removeEventListener('unhandledrejection', handleRejection);
      window.removeEventListener('error', handleError);
    };
  }, []);

  // Debounced message dismisal
  useEffect(() => {
    if (!hidden) {
      const task = setTimeout(() => setHidden(true), AUTO_HIDE_DELAY);
      return () => clearTimeout(task);
    }
  }, [hidden]);

  return (
    <div className={cx(style.root, { hidden })}>
      <div className={style.frame}>
        <div className={style.closeButton}>
          <CloseMiniButton onClick={() => setHidden(true)} />
        </div>
        {message}
      </div>
    </div>
  );
}
