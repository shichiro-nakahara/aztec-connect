import { configurator } from './configurator.js';
import { createLogger } from '@aztec/barretenberg/log';

export class Notifier {
  private endpoint: string | undefined;
  private channelId: string | undefined;
  private blockExplorer: string | undefined;
  private log;

  constructor(
    private component: string
  ) {
    this.endpoint = configurator.getConfVars().runtimeConfig.telegramSendMessageEndpoint;
    this.channelId = configurator.getConfVars().runtimeConfig.telegramChannelId;
    this.blockExplorer = configurator.getConfVars().blockExplorer;
    this.log = createLogger(`${this.component}Notifier`);

    if (this.endpoint && this.channelId) {
      this.log(`Successfully configured: ${this.endpoint} (${this.channelId})`);
      return;
    }
    
    this.log(`Incorrectly configured: ${this.endpoint} (${this.channelId})`);
  }

  public async send(message: string) {
    if (!this.endpoint || !this.channelId) return;

    const matches = message.matchAll(/\{\{(.*?)\}\}/g); // "This is a link: {{ txHash }}"
    let match;
    while (match = matches.next()) {
      if (match.done) break;
      const txHash = match.value[1].trim();

      if (this.blockExplorer) {
        message = message.replace(match.value[0], `<a href="${this.blockExplorer}/tx/${txHash}">${txHash.slice(0, 6)}...${txHash.slice(-4)}</a>`);
        continue;
      }
        
      message = message.replace(match.value[0], txHash);
    }
    
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