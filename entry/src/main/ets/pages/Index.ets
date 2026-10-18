import testNapi from 'libentry.so';
import { common } from '@kit.AbilityKit';
import { inputMethod } from '@kit.IMEKit';
import { display } from '@kit.ArkUI';


@Entry
@Component
struct Index {
  private xcompController: XComponentController = new XComponentController();
  private surfaceId: string = '';
  private renderStarted: boolean = false;
  private timerId: number = -1;
  private containerId: string = 'keyboard-container'
  @State status: string = 'idle';
  // 记录当前按住的键（keyCode），用于去重长按 repeat 事件：
  // 长按时鸿蒙会高频触发 Down（repeat），若每次都投给引擎，
  // 会反复跨 NAPI + 抢锁，打断渲染循环，导致「按住前进却一卡一卡」。
  private pressedKeys: Set<number> = new Set<number>();

  private requestFocus() {
    // 通过 UIContext 获取 FocusController 并请求焦点
    this.getUIContext().getFocusController().requestFocus(this.containerId)
  }

  aboutToAppear() {
    // 也可以在这里延迟请求焦点，确保组件已挂载
    setTimeout(() => {
      this.requestFocus()
    }, 100)
  }

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
    this.requestFocus()
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
        // 关键：必须用 changedTouches 而非 touches。
        // 鸿蒙 TouchEvent.touches 在 Up 时为空数组（手指已抬起），
        // 若用 touches 遍历，Up 事件永远投递不出去，导致：
        //   1) 拖拽视角后无法正确结束（触摸状态卡死，后续无法再切换视角）
        //   2) 松手动作丢失，弓箭/物品使用无法释放
        // changedTouches 则始终包含本次 Down/Move/Up 变化的触点。
        for (const t of event.changedTouches) {
          const x = vp2px(t.x);
          const y = vp2px(t.y);
          let action = 2;
          if (event.type === TouchType.Down) {
            action = 1;
          } else if (event.type === TouchType.Up) {
            action = 0;
          } else if (event.type === TouchType.Cancel) {
            action = 0; // Cancel 视同 Up，避免状态残留
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
    .id(this.containerId)               // 设置 ID 以便请求焦点
    .focusable(true)                    // 允许获得焦点
    .focusOnTouch(true)                 // 点击时自动获得焦点（可选）
    .onKeyEvent((event: KeyEvent) => {
      // 忽略 CANCEL（值=3）：这是系统在 Up 被阻止后补发的事件，
      // 若把它当 KEYUP 投给引擎，会把「按住移动」的状态意外清掉，导致无法长按持续行动。
      // 当前 SDK 的 KeyType 类型尚无 CANCEL 枚举，故用字面量 3 判断。
      if (event.type === 3) {
        return false
      }

      // 去重长按 repeat：Down 时若该键已在按住集合中，则跳过（重复按下）。
      // Up 时从集合移除。这样既能减少 NAPI 调用/抢锁打断渲染，又保证
      // 只向引擎投递一次 DOWN + 一次 UP，移动状态稳定不抖动。
      const isDown = (event.type === KeyType.Down)
      if (isDown) {
        if (this.pressedKeys.has(event.keyCode)) {
          return false // repeat Down，忽略
        }
        this.pressedKeys.add(event.keyCode)
      } else {
        this.pressedKeys.delete(event.keyCode)
      }

      // 仅在实际投递（非 repeat）时更新状态，避免高频 repeat 反复触发
      // @State 重渲染拖慢渲染循环（这正是长按卡顿的来源之一）。
      this.status = `KeyCode: ${event.keyCode} ${event.keyText}`

      testNapi.handleKeyBoard(event.type, event.keyCode);

      // 文本输入（聊天 / 命名 / 告示牌等）——仅在按下时投递，避免重复。
      // 同时传 keyCode：模拟器 uinput 的 keyText 是 "KEYCODE_X" 键名，
      // C++ 侧会用 keyCode 映射回退出真实字符。
      if (isDown && event.keyText && event.keyText.length > 0) {
        testNapi.handleKeyText(event.keyText, event.keyCode);
      }

      // 返回 false 让事件正常流转，避免系统因「消费了 Up」而补发 CANCEL，
      // 从而破坏长按（按住持续移动）的状态保持。
      return false
    })
    .width('100%')
    .height('100%')
    .backgroundColor('#101418')
    .expandSafeArea([SafeAreaType.SYSTEM], [SafeAreaEdge.TOP, SafeAreaEdge.BOTTOM])
  }
}
