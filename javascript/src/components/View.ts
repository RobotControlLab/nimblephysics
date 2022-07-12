import * as THREE from "three";

import { EffectComposer } from "../threejs_lib/postprocessing/EffectComposer.js";
import { SSAOPass } from "../threejs_lib/postprocessing/SSAOPass.js";
import { OrbitControls } from "../threejs_lib/controls/OrbitControls.js";
import MouseControls from './MouseControls';

class View {
  container: HTMLDivElement;
  scene: THREE.Scene;
  renderer: THREE.Renderer;
  camera: THREE.Camera;
  composer: EffectComposer;
  width: number;
  height: number;
  orbitControls: OrbitControls;
  dragControls: MouseControls;
  parent: HTMLElement;
  inScene: Map<number, THREE.Object3D>;
  tooltips: Map<number, string>;
  tooltipSets: Map<string, number[]>;
  dragEnabled: Map<number, boolean>;
  editTooltipEnabled: Map<number, boolean>;
  onTooltipHoveron: (keys: number[], tooltip: string, top_x: number, top_y: number) => void;
  onTooltipHoveroff: () => void;
  onEditTooltip: (key: number) => void;

  constructor(scene: THREE.Scene, parent: HTMLElement, onTooltipHoveron: (keys: number[], tooltip: string, top_x: number, top_y: number) => void, onTooltipHoveroff: () => void, onEditTooltip: (key: number) => void) {
    this.scene = scene;
    this.container = document.createElement("div");
    this.container.className = "View__container";
    parent.appendChild(this.container);
    this.parent = parent;
    this.onTooltipHoveron = onTooltipHoveron;
    this.onTooltipHoveroff = onTooltipHoveroff;
    this.onEditTooltip = onEditTooltip;

    // these two lines are essentially this.refreshSize(), but fix TS errors
    this.width = this.parent.getBoundingClientRect().width;
    this.height = this.parent.getBoundingClientRect().height;

    this.renderer = new THREE.WebGLRenderer();
    this.renderer.setSize(this.width, this.height);
    (this.renderer as any).shadowMap.enabled = true;
    (this.renderer as any).shadowMap.type = THREE.PCFSoftShadowMap;
    (this.renderer as any).outputEncoding = THREE.sRGBEncoding;

    this.container.appendChild(this.renderer.domElement);

    this.camera = new THREE.PerspectiveCamera(
      65,
      this.width / this.height,
      10,
      100000
    );
    this.camera.position.z = 500;

    this.composer = new EffectComposer(this.renderer);
    this.composer.setSize(this.width, this.height);

    var ssaoPass = new SSAOPass(scene, this.camera, this.width, this.height);
    ssaoPass.kernelRadius = 5;
    ssaoPass.minDistance = 0.00001;
    ssaoPass.maxDistance = 0.00006;
    this.composer.addPass(ssaoPass);
    ssaoPass.output = SSAOPass.OUTPUT.Default;

    window.addEventListener("resize", this.onWindowResize, false);

    this.orbitControls = new OrbitControls(
      this.camera,
      this.renderer.domElement
    );
    this.dragControls = new MouseControls(
      this.camera,
      this.renderer.domElement
    );
    this.tooltips = new Map();
    this.tooltipSets = new Map();
    this.inScene = new Map();
    this.dragEnabled = new Map();
    this.editTooltipEnabled = new Map();

    this.dragControls.addEventListener("dragstart", () => {
      this.orbitControls.enabled = false;
    });
    this.dragControls.addEventListener("dragend", () => {
      this.orbitControls.enabled = true;
    });
    this.dragControls.addEventListener("doubleclick", ((evt: {object: THREE.Object3D, key: number}) => {
      this.onEditTooltip(evt.key);
    }) as any)
    this.dragControls.addEventListener("hoveron", ((evt: {object: THREE.Object3D, key: number, top_x: number, top_y: number}) => {
      if (this.tooltips.has(evt.key)) {
        const tooltip = this.tooltips.get(evt.key);
        const set = this.tooltipSets.get(tooltip);
        this.onTooltipHoveron(set, this.tooltips.get(evt.key), evt.top_x, evt.top_y);
      }
    }) as any);
    this.dragControls.addEventListener("hoveroff", (() => {
      this.onTooltipHoveroff();
    }) as any);

    this.orbitControls.addEventListener("change", () => {
      this.composer.render();
    });
    this.dragControls.addEventListener("drag", () => {
      this.composer.render();
    });

    this.composer.render();
  }

  /**
   * This cleans up any resources that the view is using
   */
  dispose = () => {
    this.dragControls.dispose();
    this.orbitControls.dispose();
    window.removeEventListener("resize", this.onWindowResize, false);
  };

  setDragHandler = (
    handler: (obj: THREE.Object3D, pos: THREE.Vector3) => void,
    onFinish: (obj: THREE.Object3D) => void
  ) => {
    this.dragControls.setDragHandler(handler, onFinish);
  };

  add = (key: number, obj: THREE.Object3D) => {
    let alreadyExists = this.inScene.get(key);
    if (alreadyExists === obj) {
      return;
    }
    else if (alreadyExists != null) {
      this.scene.remove(alreadyExists);
    }
    this.inScene.set(key, obj);
    this.scene.add(obj);
    this.recomputeDragList();
    this.recomputeTooltipList();
  };

  remove = (key: number) => {
    let obj = this.inScene.get(key);
    if (obj != null) {
      this.scene.remove(obj);
      this.inScene.delete(key);
      this.recomputeDragList();
      this.recomputeTooltipList();
    }
  };

  setTooltip = (key: number, tip: string) => {
    this.tooltips.set(key, tip);
    this.recomputeTooltipSet(tip);
    this.recomputeTooltipList();
  };

  removeTooltip = (key: number) => {
    let oldTip = this.tooltips.get(key);
    this.tooltips.delete(key);
    if (oldTip != null) {
      this.recomputeTooltipSet(oldTip);
    }
    this.recomputeTooltipList();
  }

  recomputeTooltipSet = (tip: string) => {
    let list: number[] = [];
    this.tooltips.forEach((v, k) => {
      if (v === tip) {
        list.push(k);
      }
    });
    this.tooltipSets.set(tip, list);
  };

  enableDrag = (key: number) => {
    this.dragEnabled.set(key, true);
    this.recomputeDragList();
  };

  disableDrag = (key: number) => {
    this.dragEnabled.delete(key);
    this.recomputeDragList();
  };

  enableEditTooltip = (key: number) => {
    this.editTooltipEnabled.set(key, true);
    this.recomputeTooltipList();
  };

  disableEditTooltip = (key: number) => {
    this.editTooltipEnabled.delete(key);
    this.recomputeTooltipList();
  };

  recomputeDragList = () => {
    let dragList: THREE.Object3D[] = [];
    let dragKeys: number[] = [];
    this.dragEnabled.forEach((v,k) => {
      if (v) {
        let obj = this.inScene.get(k);
        if (obj != null) {
          dragKeys.push(k);
          dragList.push(obj);
        }
      }
    });
    this.dragControls.setDragList(dragList, dragKeys);
  };

  recomputeTooltipList = () => {
    let tooltipList: THREE.Object3D[] = [];
    let tooltipEditable: boolean[] = [];
    let tooltipKeys: number[] = [];
    this.tooltips.forEach((tip,k) => {
      let obj = this.inScene.get(k);
      let editable = false;
      if (this.editTooltipEnabled.has(k)) editable = this.editTooltipEnabled.get(k);

      if (obj != null) {
        tooltipKeys.push(k);
        tooltipList.push(obj);
        tooltipEditable.push(editable);
      }
    });
    this.dragControls.setTooltipList(tooltipList, tooltipKeys, tooltipEditable);
  };

  refreshSize = () => {
    this.width = this.parent.getBoundingClientRect().width;
    this.height = this.parent.getBoundingClientRect().height;
  };

  onWindowResize = () => {
    this.refreshSize();

    (this.camera as any).aspect = this.width / this.height;
    (this.camera as any).updateProjectionMatrix();

    this.renderer.setSize(this.width, this.height);
    this.composer.setSize(this.width, this.height);
    this.composer.render();
  };

  render = () => {
    this.orbitControls.update();
    this.composer.render();
  };
}

export default View;
