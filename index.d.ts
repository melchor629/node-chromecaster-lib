import * as Event from 'events';
import * as Stream from 'stream';

declare interface AudioInputOptions {
    samplerate?: 44100 | 48000 | 88200 | 96000;
    bps?: 8 | 16 | 24 | 32;
    channels?: 1 | 2;
    deviceName?: string;
    timePerFrame?: number;
}

declare interface ChromecastDeviceInfo {
    domainName: string;
    addresses: string[];
    name: string;
    type: string;
}

declare class ChromecastDevice {
    constructor(device: ChromecastDeviceInfo);
    public setWebcast(webcast: Webcast): void;
    public connect(streamName: string, cbk: (error: any | null, status?: string) => void): void;
    public connect(cbk: (error: any | null, status?: string) => void): void;
    public connect(streamName: string): Promise<string>;
    public connect(): Promise<string>;
    public getVolume(cbk: (error: any | null, volume?: number) => void): void;
    public setVolume(volume: number, cbk: (error: any | null, volume?: number) => void): void;
    public isMuted(cbk: (error: any | null, muted?: boolean) => void): void;
    public setMuted(muted: boolean, cbk: (error: any | null, muted?: boolean, volume?: number) => void): void;
    public play(cbk: (error: any | null) => void): void;
    public pause(cbk: (error: any | null) => void): void;
    public stop(cbk: (error: any | null) => void): void;
    public close();
}

declare module "chromecaster-lib" {

    export class AudioInput extends Event.EventEmitter {
        public static error(errorCode: number): string | null;
        public static getDevices(): string[];
        public static loadNativeLibrary(path: string): true;
        public static isNativeLibraryLoaded(): boolean;

        constructor(opts: AudioInputOptions);
        public open(): void;
        public close(): void;
        public pause(): void;
        public isOpen(): void;
        public isPaused(): void;

        public on(eventName: 'data', listener: (pcm: Buffer) => void);
    }

    export class ChromecastDiscover extends Event.EventEmitter {
        constructor();
        public start(): void;
        public stop(): void;
        public getDeviceAddress(name: string): string | null;
        public getDeviceNameForNumber(num: number): ChromecastDeviceInfo | null;
        public forEachClient(doSomething: (dev: ChromecastDeviceInfo) => void): void;
        public createClient(name: string | ChromecastDeviceInfo): ChromecastDevice;
    
        public on(eventName: 'device', listener: (dev: ChromecastDeviceInfo) => void);
    }

    export class Webcast extends Stream.Writable {
        public readonly localIp: string;
        public readonly contentType: string;
        public readonly port: number;
        constructor(opts: Stream.WritableOptions & { port?: number; contentType?: string; });
        public stop(): void;
    
        public on(event: 'connect', listener: (id: number, req: any) => void);
        public on(event: 'disconnect', listener: () => void);
    }

}