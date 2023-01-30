import NimbleView from "./NimbleView";
// import logoSvg from "!!raw-loader!./nimblelogo.svg";
import { CommandRecording } from "./types";
import protobuf from 'google-protobuf';
import {dart} from './proto/GUI';
import { createHash } from 'sha256-uint8array';

class NimbleStandalone {
  view: NimbleView | null;
  lastRecordingHash: string;
  rawBytes: Uint8Array;
  framePointers: number[];
  frameSizes: number[];
  playing: boolean;
  scrubbing: boolean;
  startedPlaying: number;
  originalMsPerFrame: number;
  playbackMultiple: number;
  msPerFrame: number;
  startFrame: number;
  lastFrame: number;
  scrubFrame: number;

  animationKey: number;

  viewContainer: HTMLDivElement;
  progressBarContainer: HTMLDivElement;
  progressBarBackground: HTMLDivElement;
  progressBar: HTMLDivElement;
  progressScrub: HTMLDivElement;

  loadingContainerMounted: boolean;
  loadingTitle: HTMLDivElement;
  loadingContainer: HTMLDivElement;
  loadingProgressBarContainer: HTMLDivElement;
  loadingProgressBarBg: HTMLDivElement;

  playbackSpeed: HTMLInputElement;
  playbackSpeedDisplay: HTMLDivElement;

  frameChangedListener: ((frame: number) => void) | null;
  playPausedListener: ((playing: boolean) => void) | null;

  constructor(container: HTMLElement) {
    this.frameChangedListener = null;
    this.playPausedListener = null;

    this.viewContainer = document.createElement("div");
    this.viewContainer.className = "NimbleStandalone-container";
    container.appendChild(this.viewContainer);
    this.view = new NimbleView(this.viewContainer, true);

    const instructions = document.createElement("div");
    instructions.innerHTML = "Press [Space] to Play/Pause"
    instructions.className =
      "NimbleStandalone-progress-instructions";
    this.viewContainer.appendChild(instructions);

    const playbackSpeedContainer = document.createElement("div");
    playbackSpeedContainer.className = "NimbleStandalone-playback-speed-container";
    this.viewContainer.appendChild(playbackSpeedContainer);
    this.playbackSpeed = document.createElement("input");
    this.playbackSpeed.type = 'range';
    this.playbackSpeed.min = '0.01';
    this.playbackSpeed.max = '1.5';
    this.playbackSpeed.value = '1.0';
    this.playbackSpeed.step = '0.01';
    this.playbackSpeedDisplay = document.createElement('div');
    this.playbackSpeedDisplay.innerHTML = '1.00x speed';
    playbackSpeedContainer.appendChild(this.playbackSpeedDisplay);
    playbackSpeedContainer.appendChild(this.playbackSpeed);
    this.playbackSpeed.oninput = (e: Event) => {
      const val: number = parseFloat(this.playbackSpeed.value);
      this.playbackSpeedDisplay.innerHTML = val+'x speed';
      this.setPlaybackSpeed(val);
    }

    this.progressBarContainer = document.createElement("div");
    this.progressBarContainer.className =
      "NimbleStandalone-progress-bar-container";
    this.viewContainer.appendChild(this.progressBarContainer);

    this.progressBarBackground = document.createElement("div");
    this.progressBarBackground.className = "NimbleStandalone-progress-bar-bg";
    this.progressBarContainer.appendChild(this.progressBarBackground);

    this.progressBar = document.createElement("div");
    this.progressBar.className = "NimbleStandalone-progress-bar";
    this.progressBarContainer.appendChild(this.progressBar);

    this.progressScrub = document.createElement("div");
    this.progressScrub.className = "NimbleStandalone-progress-bar-scrub";
    this.progressBarContainer.appendChild(this.progressScrub);

    const processMouseEvent = (e: MouseEvent) => {
      e.preventDefault();
      e.stopPropagation();
      const rect = this.progressBarContainer.getBoundingClientRect();
      const x = e.clientX - rect.left;
      let percentage = x / rect.width;
      if (percentage < 0) percentage = 0;
      if (percentage > 1) percentage = 1;
      if (this.playing) this.togglePlay();
      this.scrubFrame = Math.round(this.framePointers.length * percentage); 
      this.setProgress(percentage);
    };

    this.progressBarContainer.addEventListener("mousedown", (e: MouseEvent) => {
      processMouseEvent(e);

      this.scrubbing = true;
      this.startAnimation();

      window.addEventListener("mousemove", processMouseEvent);
      const mouseUp = () => {
        window.removeEventListener("mousemove", processMouseEvent);
        window.removeEventListener("mousup", mouseUp);
        this.scrubbing = false;
      };
      window.addEventListener("mouseup", mouseUp);
    });

    window.addEventListener("keydown", this.keyboardListener);

    this.view.addDragListener((key: number, pos: number[]) => {
      if (this.view != null) {
        this.view.setObjectPos(key, pos);
      }
    });

    this.lastRecordingHash = "";
    this.framePointers = [];
    this.frameSizes = [];
    this.animationKey = 0;
    this.playing = false;
    this.scrubbing = false;
    this.startedPlaying = new Date().getTime();
    this.lastFrame = -1;
    this.scrubFrame = 0;
    this.originalMsPerFrame = 20.0;
    this.playbackMultiple = 1.0;
    this.msPerFrame = this.originalMsPerFrame / this.playbackMultiple;
    this.startFrame = 0;

    this.loadingContainerMounted = false;
    this.loadingContainer = document.createElement("div");
    this.loadingContainer.className = "NimbleStandalone-loading-overlay";

    /*
    const loadingLogo = document.createElement("svg");
    loadingLogo.innerHTML = logoSvg;
    this.loadingContainer.appendChild(loadingLogo);
    */

    this.loadingTitle = document.createElement("div");
    this.loadingTitle.className = "NimbleStandalone-loading-text";
    this.loadingTitle.innerHTML = "nimble<b>viewer</b> loading...";
    this.loadingContainer.appendChild(this.loadingTitle);

    const loadingProgressBarOuterContainer = document.createElement("div");
    loadingProgressBarOuterContainer.className =
      "NimbleStandalone-loading-bar-container";
    this.loadingContainer.appendChild(loadingProgressBarOuterContainer);

    const loadingContainerInnerBg = document.createElement("div");
    loadingContainerInnerBg.className =
      "NimbleStandalone-loading-bar-container-inner-bg";
    loadingProgressBarOuterContainer.appendChild(loadingContainerInnerBg);

    this.loadingProgressBarContainer = document.createElement("div");
    this.loadingProgressBarContainer.className =
      "NimbleStandalone-loading-bar-container-inner-bar-container";
    loadingContainerInnerBg.appendChild(this.loadingProgressBarContainer);
    this.loadingProgressBarBg = document.createElement("div");
    this.loadingProgressBarBg.className =
      "NimbleStandalone-loading-bar-container-inner-bar-container-bg";
    this.loadingProgressBarContainer.appendChild(this.loadingProgressBarBg);
  }

  /**
   * This is our keyboard listener, which we keep around until we clean up the player.
   */
  keyboardListener = (e: KeyboardEvent) => {
    if (e.key.toString() == " ") {
      e.preventDefault();
      this.togglePlay();
    }
  };

  /**
   * This cleans up and kills the standalone player.
   */
  dispose = () => {
    if (this.view != null) {
      this.view.dispose();
      this.viewContainer.remove();
    }
    this.playing = false;
    this.view = null;
    window.removeEventListener("keydown", this.keyboardListener);
  };

  setProgress = (percentage: number) => {
    this.progressBar.style.width = (1.0 - percentage) * 100 + "%";
    this.progressScrub.style.left = percentage * 100 + "%";

    const zeroRGB = [255, 184, 0];
    const oneThirdRGB = [245, 71, 71];
    const twoThirdRGB = [207, 50, 158];
    const fullRGB = [141, 25, 233];

    function pickHex(color1: number[], color2: number[], weight: number) {
      var w2 = weight;
      var w1 = 1 - w2;
      return (
        "rgb(" +
        Math.round(color1[0] * w1 + color2[0] * w2) +
        "," +
        Math.round(color1[1] * w1 + color2[1] * w2) +
        "," +
        Math.round(color1[2] * w1 + color2[2] * w2) +
        ")"
      );
    }

    if (percentage < 0.33) {
      this.progressScrub.style.backgroundColor = pickHex(
        zeroRGB,
        oneThirdRGB,
        (percentage - 0.0) / 0.33
      );
    } else if (percentage < 0.66) {
      this.progressScrub.style.backgroundColor = pickHex(
        oneThirdRGB,
        twoThirdRGB,
        (percentage - 0.33) / 0.33
      );
    } else {
      this.progressScrub.style.backgroundColor = pickHex(
        twoThirdRGB,
        fullRGB,
        (percentage - 0.66) / 0.33
      );
    }
  };

  /**
   * Sets the loading text we display to users
   */
  setLoadingType = (type: string) => {
    this.loadingTitle.innerHTML = "nimble<b>viewer</b> "+type+"...";
  }

  /**
   * The loading progress
   *
   * @param progress The progress from 0-1 in loading
   */
  setLoadingProgress = (progress: number) => {
    if (!this.loadingContainerMounted) {
      this.loadingContainer.remove();
      this.viewContainer.appendChild(this.loadingContainer);
      this.loadingContainerMounted = true;
    }
    this.loadingProgressBarContainer.style.width = "calc(" + progress * 100 + "% - 6px)";
    this.loadingProgressBarBg.style.width = (1.0 / progress) * 100 + "%";
  };

  /**
   * This hides the loading bar, which unmounts it from the DOM (if it was previously mounted).
   */
  hideLoadingBar = () => {
    if (this.loadingContainerMounted) {
      this.loadingContainer.remove();
      this.loadingContainerMounted = false;
    }
  }

  /**
   * This loads a recording to play back. It attempts to display a progress bar while loading the model.
   *
   * @param url The URL to load a recording from, in order to play back
   */
  loadRecording = (url: string) => {
    this.setLoadingType("loading");
    this.setLoadingProgress(0.0);

    let xhr = new XMLHttpRequest();
    xhr.open("GET", url);

    xhr.onprogress = (event) => {
      console.log(`Received ${event.loaded} of ${event.total}`);
      this.setLoadingProgress(event.loaded / event.total);
    };

    xhr.onload = () => {
      if (xhr.status != 200) {
        console.log(`Error ${xhr.status}: ${xhr.statusText}`);
      } else {
        let response = JSON.parse(xhr.response);
        this.setRecording(response);
      }
    };

    xhr.send();
  };

  getRecordingFrame: (number) => dart.proto.CommandList = (index: number) => {
    let cursor: number = this.framePointers[index];
    // const u32bytes = this.rawBytes.buffer.slice(cursor, cursor+4); // last four bytes as a new `ArrayBuffer`
    // const size = new Uint32Array(u32bytes)[0];
    const size = this.frameSizes[index];
    cursor += 4;
    let command: dart.proto.CommandList = dart.proto.CommandList.deserialize(this.rawBytes.subarray(cursor, cursor + size));
    return command;
  };

  /**
   * This replaces the set of recorded commands we're replaying
   *
   * @param recording The JSON object representing a recording of timestep(s) command(s)
   */
  setRecording = (rawBytes: Uint8Array) => {
    const hash = createHash().update(rawBytes).digest("hex");

    if (hash !== this.lastRecordingHash) {
      this.lastRecordingHash = hash;
      this.rawBytes = rawBytes;
      this.framePointers = [];
      this.frameSizes = [];

      let cursor = [0];

      this.setLoadingType("unzipping data");
      let parseMoreBytes = () => {
        const startTime = new Date().getTime();

        if (cursor[0] < rawBytes.length) {
          while (cursor[0] < rawBytes.length) {
            // Read thet size byte
            this.setLoadingProgress(cursor[0] / rawBytes.buffer.byteLength);
            this.framePointers.push(cursor[0]);
            const u32bytes = rawBytes.buffer.slice(cursor[0], cursor[0]+4); // last four bytes as a new `ArrayBuffer`
            const size = new Uint32Array(u32bytes)[0];
            this.frameSizes.push(size);
            cursor[0] += 4;
            cursor[0] += size;

            const elapsed = new Date().getTime() - startTime;
            if (elapsed > 200) {
              break;
            }
          }
          requestAnimationFrame(parseMoreBytes);
        }
        else {
          setTimeout(() => {
            this.hideLoadingBar();
            this.setLoadingType("loading");
            this.setFrame(0);
            this.view.view.onWindowResize();
            if (!this.playing) {
              this.togglePlay();
            }
          }, 100);
        }
      };
      parseMoreBytes();
    }
    else {
      setTimeout(() => {
        this.hideLoadingBar();
      }, 100);
    }
  };

  registerPlayPauseListener = (playPausedListener: ((playing: boolean) => void) | null) => {
    this.playPausedListener = playPausedListener;
  };

  /**
   * This turns playback on or off.
   */
  togglePlay = () => {
    this.playing = !this.playing;
    if (this.playPausedListener != null) {
      this.playPausedListener(this.playing);
    }
    if (this.playing) {
      this.startFrame = this.lastFrame;
      this.startedPlaying = new Date().getTime();
      this.startAnimation();
    }
  };

  /**
   * Sets whether or not we're currently playing.
   */
  setPlaying = (playing: boolean) => {
    if (this.rawBytes != null && playing != this.playing) {
      this.playing = playing;
      if (this.playing) {
        this.startFrame = this.lastFrame;
        this.startedPlaying = new Date().getTime();
        this.startAnimation();
      }
    }
  }

  getPlaying = () => {
    return this.playing;
  }

  /**
   * This sets our playback speed to a multiple of the fundamental number for this data.
   */
  setPlaybackSpeed = (multiple: number) => {
    this.startFrame = this.getCurrentFrame();
    this.startedPlaying = new Date().getTime();

    this.playbackMultiple = multiple;
    this.msPerFrame = this.originalMsPerFrame / this.playbackMultiple;
  };

  getCurrentFrame = () => {
    const elapsed: number = new Date().getTime() - this.startedPlaying;
    const currentFrame = (this.startFrame + Math.round(elapsed / this.msPerFrame)) %
      this.framePointers.length;
    return currentFrame;
  };

  registerFrameChangeListener = (frameChange: ((frame: number) => void) | null) => {
    this.frameChangedListener = frameChange;
  };

  setFrame = (frameNumber: number) => {
    if (this.rawBytes != null && frameNumber != this.lastFrame) {
      if (frameNumber < this.lastFrame) {
        // Reset at the beginning
        this.lastFrame = -1;
        // Deliberately skip the first frame when looping back, for efficiency. The first frame usually has a bunch of creation of meshes and stuff, which is expensive to decode and hangs the browser.
        if (frameNumber == 0) {
          frameNumber = 1;
        }
      }
      this.setProgress(frameNumber / this.framePointers.length);
      if (this.view != null) {
        this.getRecordingFrame(frameNumber).command.forEach(this.handleCommand);

        // This is the slower but more correct method.
        /*
        for (let i = this.lastFrame + 1; i <= frameNumber; i++) {
          this.recording[i].command.forEach(this.handleCommand);
        }
        */
        this.view.render();
      }
      this.lastFrame = frameNumber;
    }
  };

  getFrame = () => {
    return this.lastFrame;
  };

  handleCommand = (command: dart.proto.Command) => {
    if (command.set_frames_per_second) {
      console.log("Frames per second: " + command.set_frames_per_second);
      this.originalMsPerFrame = 1000.0 / command.set_frames_per_second.framesPerSecond;
      this.msPerFrame = this.originalMsPerFrame / this.playbackMultiple;
    }
    else {
      this.view.handleCommand(command);
    }
  };

  startAnimation = () => {
    const key = Math.floor(Math.random() * 10000000);
    this.animationKey = key;
    this.animationFrame(key);
  }

  /**
   * This gets called within requestAnimationFrame(...), and handles replaying any commands necessary to jump to the appropriate time in the animation.
   */
  animationFrame = (deduplicationKey: number) => {
    // Only allow a single thread of animationFrame() calls to run at a time
    if (deduplicationKey !== this.animationKey) {
      return;
    }
    // Avoid race conditions if we were stopped, then this frame fired later
    if (!this.playing && !this.scrubbing) return;

    if (this.playing) {
      let frameNumber = this.getCurrentFrame();
      if (frameNumber != this.lastFrame) {
        this.setFrame(frameNumber);
        // Always call this _after_ updating this.lastFrame, to avoid loops
        if (this.frameChangedListener != null) {
          this.frameChangedListener(frameNumber);
        }
      }
    }
    if (this.scrubbing) {
      if (this.scrubFrame !== this.lastFrame) {
        this.setFrame(this.scrubFrame);
        // Always call this _after_ updating this.lastFrame, to avoid loops
        if (this.frameChangedListener != null) {
          this.frameChangedListener(this.scrubFrame);
        }
      }
    }

    // Don't use requestAnimationFrame(), because that causes contention with the mouse interaction, which makes the whole UI feel sluggish
    setTimeout(() => this.animationFrame(deduplicationKey), this.msPerFrame);
  };
}

export default NimbleStandalone;
