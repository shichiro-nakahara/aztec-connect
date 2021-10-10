export class WorldStateConstants {
  public static EMPTY_DATA_ROOT = Buffer.from(
    '11977941a807ca96cf02d1b15830a53296170bf8ac7d96e5cded7615d18ec607',
    'hex',
  );
  public static EMPTY_NULL_ROOT = Buffer.from(
    '1b831fad9b940f7d02feae1e9824c963ae45b3223e721138c6f73261e690c96a',
    'hex',
  );
  public static EMPTY_ROOT_ROOT = Buffer.from(
    '1b435f036fc17f4cc3862f961a8644839900a8e4f1d0b318a7046dd88b10be75',
    'hex',
  );
  public static EMPTY_DEFI_ROOT = Buffer.from(
    '0170467ae338aaf3fd093965165b8636446f09eeb15ab3d36df2e31dd718883d',
    'hex',
  );

  // value of a single empty interaction hash
  public static EMPTY_INTERACTION_HASH = Buffer.from(
    '2d25a1e3a51eb293004c4b56abe12ed0da6bca2b4a21936752a85d102593c1b4',
    'hex',
  );

  // value of a SHA256 of NUM_BRIDGE_CALLS of empty interaction hashes
  public static INITIAL_INTERACTION_HASH = Buffer.from(
    '1aea0db5ca43c22acdd5c4173782382f3abdfb601bcf12fe6eac451ad154e37d',
    'hex',
  );
}
