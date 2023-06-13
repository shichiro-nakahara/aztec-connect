import { createLogger } from '@aztec/barretenberg/log';

export class Notifier {
  private log;

  constructor(
    private component: string,
    private endpoint: string | undefined,
    private channelId: string | undefined,
  ) {
    this.log = createLogger(`${this.component}Notifier`);

    if (this.endpoint && this.channelId) {
      this.log(`Successfully configured: ${this.endpoint} (${this.channelId})`);
      return;
    }
    
    this.log(`Incorrectly configured: ${this.endpoint} (${this.channelId})`);
  }

  public async send(message: string) {
    if (!this.endpoint || !this.channelId) return;
    
    message = `<b>${this.component}</b>\n\n${message}`;

    try {
      const res = await fetch(`${this.endpoint}?` + new URLSearchParams({
        text: message,
        parse_mode: 'HTML',
        chat_id: this.channelId,
        disable_web_page_preview: 'true'
      }));
      
      if (res.status != 200) this.log(`Failed to notify (${res.status}): ${res.statusText}`);
    } catch (e) {
      this.log(`Failed to notify: ${e}`);
    }
  }
}