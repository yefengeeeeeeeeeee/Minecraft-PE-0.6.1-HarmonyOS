import testNapi from 'libentry.so';
import { common } from '@kit.AbilityKit';
import { display } from '@kit.ArkUI';

@Entry
@Component
struct Index {
  private xcompController: XComponentController = new XComponentController();
  private surfaceId: string = '';
  private renderStarted: boolean = false;
  private timerId: number = -1;
  @State status: string = 'idle';

  aboutToDisappear(): void {
    if (this.timerId >= 0) {
      clearInterval(this.timerId);
      this.timerId = -1;
    }
    try {
      testNapi.destroyRender();
    } catch (e) {
      this.status = 'destroy error: ' + JSON.stringify(e);
    }
  }

  onSurfaceLoad(): void {
    const sid = this.xcompController.getXComponentSurfaceId();
    this.surfaceId = sid;
    this.status = 'surfaceId=' + sid;
    this.startRender();
  }

  startRender(): void {
    if (this.renderStarted || this.surfaceId === '') {
      return;
    }
    try {
      const ctx = getContext(this) as common.UIAbilityContext;
      const resMgr = ctx.resourceManager;
      const rect = this.xcompController.getXComponentSurfaceRect();
      const screenW = rect.surfaceWidth;
      const screenH = rect.surfaceHeight;
      const filesDir = ctx.filesDir;
      this.status = 'initRender...';
      testNapi.initRender(resMgr, this.surfaceId, screenW, screenH, filesDir);
      this.renderStarted = true;
      this.status = 'running ' + screenW + 'x' + screenH;
      this.timerId = setInterval(() => {
        testNapi.renderFrame();
      }, 16);
    } catch (e) {
      this.status = 'init error: ' + JSON.stringify(e);
    }
  }

  build() {
    Stack() {
      XComponent({
        id: 'xcomp',
        type: 'surface',
        controller: this.xcompController
      })
      .onLoad(() => {
        this.onSurfaceLoad();
      })
      .onTouch((event: TouchEvent) => {
        for (const t of event.touches) {
          const x = vp2px(t.x);
          const y = vp2px(t.y);
          let action = 2;
          if (event.type === TouchType.Down) {
            action = 1;
          } else if (event.type === TouchType.Up) {
            action = 0;
          }
          testNapi.renderTouch(x, y, action, t.id);
        }
      })
      .width('100%')
      .height('100%')
      .backgroundColor('#1A202C')

      Text(this.status)
        .fontSize(11)
        .fontFamily('monospace')
        .fontColor('#8CE99A')
        .backgroundColor('#80000000')
        .padding(4)
        .position({ x: 4, y: 4 })
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#101418')
    .expandSafeArea([SafeAreaType.SYSTEM], [SafeAreaEdge.TOP, SafeAreaEdge.BOTTOM])
  }
}
