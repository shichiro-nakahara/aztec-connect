export interface Fft {
  fft(coefficients: Uint8Array, constant: Uint8Array): Promise<Uint8Array>;
  ifft(coefficients: Uint8Array): Promise<Uint8Array>;
}
