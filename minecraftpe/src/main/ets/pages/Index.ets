import testNapi from 'libminecraftpe.so';
import { common } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';

const LOG_DOMAIN: number = 0x0001;
const LOG_TAG: string = 'MCPE-KB';
// 排查按键问题时改成 true，会在 hilog 里打印每一个原始按键事件
const DEBUG_KB: boolean = false;

// 与 main_ohos.cpp 里 OhosKeyCodeToMcpe() 对应的鸿蒙键码（@ohos.multimodalInput.keyCode）
const KEYCODE_ENTER: number = 2054;
const KEYCODE_DEL: number = 2055;
// 隐藏输入框里镜像文本的上限：超过就清空重来，避免越积越长
const IME_MIRROR_LIMIT: number = 160;
// 「哨兵」字符：零宽空格，界面上看不见，也不会被送进引擎（C++ 侧只收 ASCII）。
// 它常驻在隐藏输入框里，保证输入框永远不是空的 —— 否则在「设置」里给已经填好内容的
// 文本框按退格时，输入框本来是空的，删不出任何变化、onChange 不触发，
// 引擎就永远收不到 Backspace，表现为「设置里删不掉字」。
const IME_SENTINEL: string = '\u200B';
// 点了输入法的「发送」之后，这段时间内不许因为「焦点丢了」而重新拉起键盘。
// 输入法在发送时会立刻收键盘，而引擎要到下一帧才处理 ENTER、把 want 置回 false，
// 这段空档里补焦点就会把键盘又弹出来（沉一下又冒出来）。
const IME_SUBMIT_QUIET_MS: number = 260;
// 长按输入的两个时间参数。
//
// 背景（实测日志）：鸿蒙的按键事件流并不总是「一次 Down … 一次 Up」。
// 按住某个键时，系统/输入通道经常把一次长按拆成很多组 Down/Up 成对事件；
// 更糟的是，按下第二个键时，第一个键还可能被顺手补一个 Up。
// 引擎的移动状态是「收到 Up 就停」，所以这些多出来的 Up 会让角色：
//   1) 走一下停一下（停顿）；
//   2) 按住前进再去按跳跃时，前进被那个假 Up 清掉，两个键没法同时生效。
//
// 对策：Up 一律先挂起，只有确认「真的松手」才告诉引擎。
//   - 用户自己松手：挂起 KEYUP_GRACE_MS，期间同键又来 Down 才撤销（桥接 Down/Up 抖动）。
//     这种挂起**不允许**被「按下别的键」撤销 —— 否则按跳跃的那一下会把前进的松开吃掉，
//     前进键从此永久按住（这正是「不按角色也一直往前走」的根因）。
//   - 疑似假 Up（刚刚按下的是别的键）：挂 ARTIFACT_HOLD_MS，期间同键的 repeat Down 会撤销；
//     被「按下别的键」碰到时也只是重新挂一个短窗口，绝不会变成无限期按住。
// 另外还有两道保险：CROSS_RESIST_MS / PENDING_MAX_MS 限制单个抬起的总挂起时长，
// KEY_LEASE_MS 兜底「Up 在分发链路里彻底丢失」的情况。
const KEYUP_GRACE_MS: number = 90;
const CROSS_KEY_WINDOW_MS: number = 45;
const ARTIFACT_HOLD_MS: number = 250;
// 「跨键撤销」的耐心上限：某个键的抬起已经被别的键作废过一次以上、累计超过这么久，
// 就再也不能被作废，必须真正释放。没有这条兜底，一次误判就能把这个键永久按在
// 引擎里（表现：松开所有键后角色自己一直往前走）。
const CROSS_RESIST_MS: number = 300;
// 挂起抬起的绝对上限：到点无论处于什么状态都强制释放，防止定时器被主线程卡住
// 或者被反复重新挂起而永远不松手。
const PENDING_MAX_MS: number = 700;
// 「按键租约」：一根手指/一个键的真实长按，系统会持续补发 repeat Down（实测 20~60ms 一次）。
// 若某个键在按住集合里，却连续这么久没有任何该键的事件，说明它的 Up 在分发过程中
// 整个丢了（真机上偶发，尤其是双键混按 + 系统补发 CANCEL 时）——直接强制释放。
// 阈值取得足够大，正常长按不会误伤；万一发了 Down 又丢 Up，最多卡 3 秒就能自愈。
const KEY_LEASE_MS: number = 3000;
// 点按识别：Up 挂起后这么久之内又来了同键 Down，才算「长按的重复抖动」；
// 超过这个间隔就是用户真的松手后又按了一下（连续点按跳跃/前进）。
// 不区分的话，点按会被当成 repeat 撤销抬起，引擎里那个键永远不松 —— 角色一直走/一直跳。
const TAP_GAP_MS: number = 70;
// 需要「持续按住」语义的键：W A S D、空格、Shift、Ctrl、方向键（鸿蒙键码）。
// 只有这些键做 Up 去抖 + 每帧补按；E / T / F3 这类一次性功能键不行，
// 补按会让界面被反复开关。
const HOLD_KEY_CODES: number[] = [
  2039, 2017, 2035, 2020,   // W A S D
  2050,                     // SPACE
  2047, 2048,               // SHIFT_L / SHIFT_R
  2072, 2073,               // CTRL_L / CTRL_R
  2012, 2013, 2014, 2015    // DPAD 上下左右
];

@Entry
@Component
struct Index {
  private xcompController: XComponentController = new XComponentController();
  private surfaceId: string = '';
  private renderStarted: boolean = false;
  private timerId: number = -1;
  private containerId: string = 'keyboard-container';
  @State status: string = 'idle';
  // 引擎视角的「按住集合」：按下的键在确认抬起前一直留在这里。
  // 渲染循环每帧用它跟引擎核对状态，掉状态就补按 —— 这是移动「走走停停」的兜底。
  private heldKeys: Set<number> = new Set<number>();
  // 挂起中的 Up：keyCode -> { timerId, armedAt }
  private releaseTimers: Map<number, number> = new Map<number, number>();
  private releaseArmedAt: Map<number, number> = new Map<number, number>();
  // 某个键「第一次」挂起抬起的时刻：用来限制跨键撤销的累计次数/时长，
  // 以及挂起时长的绝对上限（见 CROSS_RESIST_MS / PENDING_MAX_MS）。
  private releaseChainStart: Map<number, number> = new Map<number, number>();
  // 每个键最后一次收到事件（Down / Up / Cancel）的时刻 —— 按键租约的计时依据
  private lastEventAt: Map<number, number> = new Map<number, number>();
  // 最近一次「真正按下」的是哪个键。用来排除一种致命误判：
  // Down 之后很短时间内到达的 Up，过去一律当成「框架补发的假 Up」，
  // 但这其实也可能是用户自己的快速点按 —— 一旦被误判，这次抬起就挂 400ms，
  // 期间只要再按别的键（跳跃）就会被撤销，前进键从此永久按住。
  private lastForeignDownKey: number = -1;
  // 被判为「框架补发的假 Up」的键：只有这些挂起允许被「按下别的键」撤销。
  // 正常抬起绝不允许被撤销 —— 松手必须真的能停下来。
  private crossCancelable: Set<number> = new Set<number>();
  // 最近一次「别的键」按下的时刻：用来识别「按新键时框架补发的假 Up」
  private lastForeignDownAt: number = 0;
  // 帧率统计（1 秒刷新一次），排查「卡顿 / 走走停停」时直接看左上角数字
  private frameCount: number = 0;
  private fpsWindowStart: number = 0;
  private fps: number = 0;

  // ---------- 屏幕键盘 / 文本输入 ----------
  // 只有「可编辑组件」拿到焦点，系统才会弹出屏幕键盘；游戏画面是 XComponent，
  // 不是可编辑组件，所以必须额外放一个 1x1 的隐藏输入框来承接焦点。
  // 它不负责显示，只把用户敲的字符按「增量」转给引擎（Keyboard::feedText）。
  private imeInputId: string = 'mcpe-ime-input';
  private imeController: TextInputController = new TextInputController();
  @State imeText: string = '';
  private imeMirror: string = '';          // 已经同步给引擎的内容
  private imeWritten: string = '';         // 我们最近一次主动回写到输入框的文本（用于识别回声回调）
  private keyboardShown: boolean = false;  // ArkTS 侧当前是否已经弹出键盘
  private imeFocused: boolean = false;
  private imeRefocusTries: number = 0;
  // 刚点过输入法的「发送」：这段时间内即使焦点因为输入法自动收起而丢失，
  // 也不许把键盘再拉起来。
  //
  // 起因：onSubmit 只是把 ENTER 投给引擎，引擎要到下一帧 renderFrame 才处理
  // （ConsoleScreen 关屏 / TextBox 丢焦点 → hideKeyboard）。可是输入法自己
  // 在「发送」时会立刻下沉，中间这 8~16ms 里 want 还是 true、imeFocused 已是
  // false，于是渲染循环判定「焦点丢了」又 showIme() 一次 —— 键盘沉下去又弹回来。
  private imeSuppressUntil: number = 0;
  // 键盘刚收起时要盯一段时间焦点：stopEditing() 是异步的，紧接着
  // requestFocus(容器) 有时会被输入法的收尾流程盖掉，焦点最终仍留在
  // 那个看不见的输入框上 —— 之后物理键盘的按键全部丢失（表现就是
  // 「聊过一次天之后键盘就走不动了」）。这里做几帧重试兜底。
  private focusWatchFrames: number = 0;

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
        // 渲染循环顺便轮询「引擎要不要键盘」，开销极小，
        // 也免去让 C++ 反向回调 ArkTS（那需要 threadsafe function，复杂且易踩线程坑）。
        this.syncSoftKeyboard();
        // 按住状态自愈 + 帧率统计
        this.syncHeldKeys();
        this.tickFps();
      }, 16);
    } catch (e) {
      this.status = 'init error: ' + JSON.stringify(e);
    }
  }

  // ---------- 屏幕键盘联动 ----------

  private showIme(): void {
    // 真的要输入了：撤掉「刚发送过」的静默窗口，之后焦点丢了就正常补
    this.imeSuppressUntil = 0;
    // 要让输入框拿焦点，就必须把焦点从游戏容器上挪走；
    // 这里先主动把所有按住键松开，免得「按着 W 打开聊天」后角色一直往前走。
    this.releaseAllHeld();
    // 先让输入框只留哨兵：这样用户第一次按退格也一定能删掉「东西」，
    // 从而触发 onChange，把 Backspace 送到引擎。
    this.imeMirror = '';
    this.imeText = IME_SENTINEL;
    this.imeWritten = IME_SENTINEL;
    try {
      // 让隐藏输入框拿到焦点 → 系统自动弹出屏幕键盘
      this.getUIContext().getFocusController().requestFocus(this.imeInputId);
    } catch (e) {
      this.status = 'ime focus error: ' + JSON.stringify(e);
    }
  }

  private hideIme(): void {
    this.resetImeMirror();
    try {
      this.imeController.stopEditing();   // 通知系统收起屏幕键盘
    } catch (e) {
      this.status = 'ime stop error: ' + JSON.stringify(e);
    }
    this.imeFocused = false;
    // 键盘收起后把焦点还给游戏容器，否则物理键盘的按键会没人处理。
    // 收起动作是异步的，紧接着的 requestFocus 可能被输入法收尾覆盖，
    // 所以这里先要一次，再加两次延迟重试；渲染循环里还有一段兜底
    // （见 syncSoftKeyboard 的 focusWatchFrames）。
    this.focusWatchFrames = 140;   // ≈2.2s @60fps
    this.requestFocus();
    setTimeout(() => {
      if (!this.keyboardShown) this.requestFocus();
    }, 90);
    setTimeout(() => {
      if (!this.keyboardShown) this.requestFocus();
    }, 320);
  }

  private syncSoftKeyboard(): void {
    let want: boolean = false;
    try {
      want = testNapi.isKeyboardRequested();
    } catch (e) {
      return;
    }

    if (want !== this.keyboardShown) {
      this.keyboardShown = want;
      this.imeRefocusTries = 0;
      if (want) {
        this.focusWatchFrames = 0;   // 输入法要接管焦点，别再抢回来
        this.resetImeMirror();
        this.showIme();
      } else {
        this.hideIme();
      }
      this.status = want ? 'softkeyboard: on' : 'softkeyboard: off';
      return;
    }

    // 引擎仍然要键盘，但输入框焦点丢了（例如手点到了画面）：
    // 补一次焦点。次数有限，避免和用户/系统抢焦点形成死循环。
    // 刚点过「发送」的那一小段时间例外：那是输入法自己在收键盘，
    // 这时候补焦点会把刚要收起的键盘又弹回来（沉一下又冒出来）。
    if (want && !this.imeFocused && this.imeRefocusTries < 3
        && Date.now() >= this.imeSuppressUntil) {
      this.imeRefocusTries++;
      this.showIme();
      return;
    }

    // 兜底：键盘已经不需要了，但焦点可能还停在那只 1x1 的输入框上。
    // 这时物理键盘的事件全部被输入框吃掉，游戏里既不能走也不能转视角。
    // 每 8 帧要一次焦点，持续约 2 秒；容器自己已是焦点时 requestFocus 是空操作。
    if (!want && this.focusWatchFrames > 0) {
      if ((this.focusWatchFrames % 8) === 0) {
        this.requestFocus();
      }
      this.focusWatchFrames--;
    }
  }

  // 给引擎补发一次「按下 + 抬起」，模拟一次键击（退格 / 回车都靠它）
  private sendKeyTap(keyCode: number): void {
    testNapi.handleKeyBoard(0, keyCode);  // KeyType.Down
    testNapi.handleKeyBoard(1, keyCode);  // KeyType.Up
  }

  private resetImeMirror(): void {
    this.imeMirror = '';
    this.imeText = IME_SENTINEL;
    this.imeWritten = IME_SENTINEL;
  }

  // 隐藏输入框的内容变化 → 换算成增量交给引擎。
  // 引擎侧没有「整串替换」的接口，只有 charPressed / Backspace，
  // 所以这里按 diff 拆成「追加字符」和「退格若干次」两种投递。
  private handleImeInput(value: string): void {
    if (value === this.imeWritten) {
      // 我们自己回写 imeText 时触发的回调，直接忽略，避免递归
      return;
    }

    const raw = value.split(IME_SENTINEL).join('');   // 去掉哨兵后的真实输入
    const prev = this.imeMirror;

    if (raw === prev) {
      // 只删掉了哨兵 → 这是一次单纯的退格。
      // 此时引擎里的文本框可能还有自己的内容（设置页里的选项都是预填好的），
      // 必须把这次 Backspace 投递过去，而不是什么都不做。
      this.sendKeyTap(KEYCODE_DEL);
    } else if (raw.length > prev.length && raw.startsWith(prev)) {
      // 正常往后追加（最常见）：大小写原样保留，Shift 打出来的就是大写
      testNapi.feedText(raw.substring(prev.length));
    } else if (raw.length < prev.length && prev.startsWith(raw)) {
      // 退格删除
      const removed = prev.length - raw.length;
      for (let i = 0; i < removed; i++) {
        this.sendKeyTap(KEYCODE_DEL);
      }
    } else {
      // 光标跳到中间插入 / 整串被替换（输入法上屏、选中后覆盖）：
      // 先把旧内容退掉，再补新的
      for (let i = 0; i < prev.length; i++) {
        this.sendKeyTap(KEYCODE_DEL);
      }
      if (raw.length > 0) {
        testNapi.feedText(raw);
      }
    }

    this.imeMirror = raw;

    // 立刻把哨兵补回去，并让光标停在末尾。
    // 哨兵必须一直在输入框里，否则下一次退格又会「删无可删」而不产生回调。
    const next = IME_SENTINEL + raw;
    this.imeText = next;
    this.imeWritten = next;
    setTimeout(() => {
      try {
        this.imeController.caretPosition(next.length);
      } catch (e) {
      }
    }, 0);

    if (next.length > IME_MIRROR_LIMIT) {
      this.resetImeMirror();
    }
  }

  // ---------- 按住状态（移动键必须连续按住） ----------

  private isHoldKey(keyCode: number): boolean {
    return HOLD_KEY_CODES.indexOf(keyCode) >= 0;
  }

  // 只在引擎里状态确实丢了的时候补一次按下，否则什么都不做
  private ensureKeyDown(keyCode: number): void {
    let down: boolean = true;
    try {
      down = testNapi.isKeyDown(keyCode);
    } catch (e) {
      return;
    }
    if (!down) {
      if (DEBUG_KB) {
        hilog.info(LOG_DOMAIN, LOG_TAG, 'REKEY code=%{public}d', keyCode);
      }
      testNapi.handleKeyBoard(0, keyCode);   // KeyType.Down
    }
  }

  // 每帧核对：按住中的键在引擎里是否还是按下。不是就补按。
  private syncHeldKeys(): void {
    if (this.heldKeys.size === 0) {
      return;
    }
    const now = Date.now();
    this.heldKeys.forEach((keyCode: number) => {
      // 按键租约：真长按会一直有 repeat Down 续租。超过租期说明这个键的 Up
      // 在分发链路里整个丢了（真机上偶发），直接释放，保证不会「松手还在走」。
      const last: number | undefined = this.lastEventAt.get(keyCode);
      if (last !== undefined && now - last > KEY_LEASE_MS) {
        if (DEBUG_KB) {
          hilog.info(LOG_DOMAIN, LOG_TAG, 'LEASE-EXPIRE code=%{public}d idle=%{public}d',
                     keyCode, now - last);
        }
        this.forceRelease(keyCode);
        return;
      }
      // 正在等「确认抬起」的键不补，避免和真正的松手打架。
      // 但等待时间不能无限长：一旦超过绝对上限就强制释放，
      // 否则「挂起 → 被撤销 → 再挂起」的循环会把这个键永久按在引擎里。
      if (this.releaseTimers.has(keyCode)) {
        const chainStart: number | undefined = this.releaseChainStart.get(keyCode);
        if (chainStart !== undefined && now - chainStart > PENDING_MAX_MS) {
          this.forceRelease(keyCode);
        }
        return;
      }
      this.ensureKeyDown(keyCode);
    });
  }

  // 不管现在挂起的是什么状态，直接认定这个键已经松开
  private forceRelease(keyCode: number): void {
    if (DEBUG_KB) {
      hilog.info(LOG_DOMAIN, LOG_TAG, 'FORCE code=%{public}d held=%{public}d', keyCode, this.heldKeys.size);
    }
    const timer: number | undefined = this.releaseTimers.get(keyCode);
    if (timer !== undefined) {
      clearTimeout(timer);
    }
    this.releaseTimers.delete(keyCode);
    this.releaseArmedAt.delete(keyCode);
    this.releaseChainStart.delete(keyCode);
    this.lastEventAt.delete(keyCode);
    this.crossCancelable.delete(keyCode);
    if (this.heldKeys.delete(keyCode)) {
      try {
        testNapi.handleKeyBoard(1, keyCode);   // KeyType.Up
      } catch (e) {
      }
    }
  }

  // 撤销某个键挂起的 Up（= 认定它还在按着）
  private cancelPendingRelease(keyCode: number, keepChain: boolean = false): boolean {
    const timer: number | undefined = this.releaseTimers.get(keyCode);
    if (timer === undefined) {
      return false;
    }
    clearTimeout(timer);
    this.releaseTimers.delete(keyCode);
    this.releaseArmedAt.delete(keyCode);
    this.crossCancelable.delete(keyCode);
    if (!keepChain) {
      this.releaseChainStart.delete(keyCode);
    }
    return true;
  }

  // 挂起一次 Up：delay 毫秒内没有该键的新事件，才真正告诉引擎「松开了」。
  // crossable=true 表示这次抬起「可能是框架补发的假 Up」，允许被「按下别的键」撤销；
  // 用户自己松手（crossable=false）的抬起一律不允许撤销 —— 松手必须真的能停下来。
  private armRelease(keyCode: number, delay: number, crossable: boolean = false): void {
    this.cancelPendingRelease(keyCode, true);
    if (crossable) {
      this.crossCancelable.add(keyCode);
    } else {
      this.crossCancelable.delete(keyCode);
    }
    const now = Date.now();
    // 记录这条键「第一次」被挂起的时刻，后续重新挂起不会重置它，
    // 这样累计时长才能真实反映「这个抬起已经被拖了多久」。
    if (!this.releaseChainStart.has(keyCode)) {
      this.releaseChainStart.set(keyCode, now);
    }
    let chainStart: number = now;
    const storedChain: number | undefined = this.releaseChainStart.get(keyCode);
    if (storedChain !== undefined) {
      chainStart = storedChain;
    }
    // 不管传进来的 delay 多长，总等待时间都不允许超过绝对上限
    let wait: number = delay;
    const remain: number = PENDING_MAX_MS - (now - chainStart);
    if (remain < wait) {
      wait = remain > 0 ? remain : 0;
    }
    const timer = setTimeout(() => {
      this.releaseTimers.delete(keyCode);
      this.releaseArmedAt.delete(keyCode);
      this.releaseChainStart.delete(keyCode);
      this.crossCancelable.delete(keyCode);
      if (this.heldKeys.delete(keyCode)) {
        if (DEBUG_KB) {
          hilog.info(LOG_DOMAIN, LOG_TAG, 'ARM-FIRE code=%{public}d', keyCode);
        }
        testNapi.handleKeyBoard(1, keyCode);   // KeyType.Up
      }
    }, wait);
    this.releaseTimers.set(keyCode, timer);
    this.releaseArmedAt.set(keyCode, now);
    if (DEBUG_KB) {
      hilog.info(LOG_DOMAIN, LOG_TAG, 'ARM code=%{public}d delay=%{public}d wait=%{public}d chain=%{public}d',
                 keyCode, delay, wait, now - chainStart);
    }
  }

  private onKeyDown(keyCode: number, keyText: string): void {
    const now = Date.now();
    this.lastEventAt.set(keyCode, now);

    // 0) 先量一下「这个键的抬起已经挂了多久」：用来区分长按抖动和真正的点按
    let pendingMs: number = -1;
    const armedAtPrev: number | undefined = this.releaseArmedAt.get(keyCode);
    if (this.releaseTimers.has(keyCode) && armedAtPrev !== undefined) {
      pendingMs = now - armedAtPrev;
    }

    // 1) 这个键自己挂起的 Up 作废
    this.cancelPendingRelease(keyCode);

    // 2) 长按 repeat：引擎那边已经是按下状态，不重复投递。
    //    必须在这里就返回 —— 下面的「跨键撤销」只允许由「真正按下另一个键」触发。
    //
    //    这里曾经是「松开前进键后角色还在自己走」的根因：
    //    按住 W + 空格时，空格每秒几十次 repeat Down 全都走进了跨键撤销，
    //    把 W 刚挂起的抬起一次次作废；于是松开 W 之后引擎里 W 仍然是按下状态，
    //    角色就一直往前走，而且永远停不下来。
    if (this.heldKeys.has(keyCode)) {
      // 抬起挂了 70ms 以上又出现 Down：这不是系统补发的长按抖动，
      // 而是用户「松手后又按了一次」。必须先把真正的抬起送给引擎再重新按下，
      // 否则引擎会认为这个键从来没松过 —— 连续点按就会变成一直走 / 一直跳。
      if (pendingMs > TAP_GAP_MS) {
        if (DEBUG_KB) {
          hilog.info(LOG_DOMAIN, LOG_TAG, 'TAP code=%{public}d gap=%{public}d', keyCode, pendingMs);
        }
        this.forceRelease(keyCode);
        this.lastEventAt.set(keyCode, now);
        // 落到下面「首次按下」流程重新按下
      } else {
        if (DEBUG_KB) {
          hilog.info(LOG_DOMAIN, LOG_TAG, 'DOWN-rep code=%{public}d', keyCode);
        }
        this.ensureKeyDown(keyCode);
        return;
      }
    }

    // 3) 真正按下了另一个键：处理那些「疑似假 Up」的挂起。
    //    只认 45ms 内挂起、且确实是「别的键按下之后才收到的 Up」（crossCancelable）。
    //    处理方式是把挂起重置成一个短窗口，而不是取消 ——
    //    取消就等于把这个键永久按在引擎里，那正是「松开后角色自己一直走」。
    if (this.releaseArmedAt.size > 0) {
      const toCancel: number[] = [];
      const toForce: number[] = [];
      this.releaseArmedAt.forEach((armedAt: number, other: number) => {
        if (other === keyCode) {
          return;
        }
        // 只有「疑似假 Up」的挂起才能被撤销。用户自己松手挂起的抬起一律不许动，
        // 否则按下跳跃的那一下就会把前进的松开吃掉，导致松手后还在走。
        if (!this.crossCancelable.has(other)) {
          return;
        }
        if (now - armedAt > CROSS_KEY_WINDOW_MS) {
          return;
        }
        let chainStart: number = armedAt;
        const stored: number | undefined = this.releaseChainStart.get(other);
        if (stored !== undefined) {
          chainStart = stored;
        }
        if (now - chainStart > CROSS_RESIST_MS) {
          toForce.push(other);
        } else {
          toCancel.push(other);
        }
      });
      for (const other of toCancel) {
        // 关键：不要「撤销」这个抬起（那会让该键永久按住），
        // 只是重新挂一个短的去抖窗口 —— 真还在按的话，它的 repeat Down 会续上；
        // 真松手了就 90ms 后停。这样最多多走 0.18 秒，绝不会卡住。
        this.armRelease(other, KEYUP_GRACE_MS, false);
      }
      for (const other of toForce) {
        this.forceRelease(other);
      }
    }
    // 只有「真正按下」才刷新这个时刻。repeat 也刷新会让松开任何键时
    // 都被误判成「假 Up」，要白等 400ms 才松手。
    this.lastForeignDownAt = now;
    this.lastForeignDownKey = keyCode;

    this.heldKeys.add(keyCode);
    if (DEBUG_KB) {
      hilog.info(LOG_DOMAIN, LOG_TAG, 'DOWN-new code=%{public}d held=%{public}d', keyCode, this.heldKeys.size);
    }
    testNapi.handleKeyBoard(0, keyCode);   // KeyType.Down

    // 文本输入（聊天 / 命名 / 告示牌等）——仅在首次按下时投递。
    // 同时传 keyCode：模拟器 uinput 的 keyText 是 "KEYCODE_X" 键名，
    // C++ 侧会用 keyCode 映射回退出真实字符（并按 Shift 补大小写）。
    // 注意：隐藏输入框正在接管输入（拿到了焦点）时，字符由它的 onChange
    // 统一投递，这里必须跳过，否则同一个字符会被投递两次。
    if (!this.imeFocused && keyText && keyText.length > 0) {
      testNapi.handleKeyText(keyText, keyCode);
    }
  }

  // 引擎侧可能还残留「按下」状态（比如之前被判成假 Up、又被清出了集合）：
  // 补一次 Up，避免「松手后角色还在走」。
  private forceEngineUpIfDown(keyCode: number): void {
    if (!this.isHoldKey(keyCode)) {
      return;
    }
    let down: boolean = false;
    try {
      down = testNapi.isKeyDown(keyCode);
    } catch (e) {
      return;
    }
    if (down) {
      try {
        testNapi.handleKeyBoard(1, keyCode);   // KeyType.Up
      } catch (e) {
      }
    }
  }

  private onKeyUp(keyCode: number): void {
    this.lastEventAt.set(keyCode, Date.now());
    if (!this.heldKeys.has(keyCode)) {
      // 集合里已经没有这个键了，但引擎里可能还按着 —— 补一次 Up 收尾。
      if (DEBUG_KB) {
        hilog.info(LOG_DOMAIN, LOG_TAG, 'UP-nokey code=%{public}d', keyCode);
      }
      this.forceEngineUpIfDown(keyCode);
      return;
    }
    if (this.releaseTimers.has(keyCode)) {
      if (DEBUG_KB) {
        hilog.info(LOG_DOMAIN, LOG_TAG, 'UP-dup code=%{public}d', keyCode);
      }
      return;
    }

    // 功能键（E / T / F3 …）立刻生效，否则会显得手感发黏
    if (!this.isHoldKey(keyCode)) {
      if (this.heldKeys.delete(keyCode)) {
        testNapi.handleKeyBoard(1, keyCode);   // KeyType.Up
      }
      return;
    }

    const now = Date.now();

    // 只有「刚刚按下的是别的键」时，这个 Up 才可能是框架补发的假 Up。
    // 如果刚刚按下的就是它自己，那这是用户的快速点按（点跳跃 / 点前进），必须当真 ——
    // 过去没有这层判断，短促点按会被误判成假 Up，挂 400ms 后才松手，
    // 期间再按别的键还会被撤销，角色就一直走个不停。
    const fromOtherKey: boolean = (this.lastForeignDownKey !== keyCode)
      && (now - this.lastForeignDownAt <= CROSS_KEY_WINDOW_MS);

    if (fromOtherKey) {
      if (DEBUG_KB) {
        hilog.info(LOG_DOMAIN, LOG_TAG, 'artificial-up code=%{public}d foreignKey=%{public}d',
                   keyCode, this.lastForeignDownKey);
      }
      this.armRelease(keyCode, ARTIFACT_HOLD_MS, true);
      return;
    }

    // 用户自己松手：挂起一小段时间桥接抖动，但不允许被别的键撤销 —— 松手必须真停
    if (DEBUG_KB) {
      hilog.info(LOG_DOMAIN, LOG_TAG, 'UP-normal code=%{public}d sinceForeign=%{public}d',
                 keyCode, now - this.lastForeignDownAt);
    }
    this.armRelease(keyCode, KEYUP_GRACE_MS, false);
  }

  // 主动松开所有按住键（打开输入法 / 切后台时用）。
  // 注意：不要挂在容器的 onBlur 上 —— 焦点在渲染帧之间偶尔会抖一下，
  // 那会把玩家正按着的 W 一起松掉，表现成「按着前进突然停住」。
  private releaseAllHeld(): void {
    if (DEBUG_KB) {
      hilog.info(LOG_DOMAIN, LOG_TAG, 'releaseAllHeld n=%{public}d', this.heldKeys.size);
    }
    this.releaseTimers.forEach((timer: number) => {
      clearTimeout(timer);
    });
    this.releaseTimers.clear();
    this.releaseArmedAt.clear();
    this.releaseChainStart.clear();
    this.lastEventAt.clear();
    this.crossCancelable.clear();
    this.heldKeys.forEach((keyCode: number) => {
      testNapi.handleKeyBoard(1, keyCode);   // KeyType.Up
    });
    this.heldKeys.clear();
  }

  // 页面切到后台（Home 键 / 切应用）：把按住键全部松开，
  // 否则回来时角色会一直朝某个方向走。
  onPageHide(): void {
    this.releaseAllHeld();
  }

  private tickFps(): void {
    const now = Date.now();
    this.frameCount++;
    if (this.fpsWindowStart === 0) {
      this.fpsWindowStart = now;
      return;
    }
    const elapsed = now - this.fpsWindowStart;
    if (elapsed < 1000) {
      return;
    }
    this.fps = Math.round(this.frameCount * 1000 / elapsed);
    this.frameCount = 0;
    this.fpsWindowStart = now;
    if (this.heldKeys.size === 0) {
      // 按住按键时保留 KeyCode 提示，方便排查键位；空闲时显示帧率
      this.status = 'fps ' + this.fps;
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

      // 1x1 隐藏输入框：只为拿焦点触发系统输入法，不参与显示。
      // opacity(0) 不影响获取焦点，但绝对不要用 visibility(Hidden)（那样不可聚焦）。
      TextInput({ text: $$this.imeText, controller: this.imeController })
        .id(this.imeInputId)
        .width(1)
        .height(1)
        .opacity(0.01)
        .padding(0)
        .borderWidth(0)
        .backgroundColor(Color.Transparent)
        .caretColor(Color.Transparent)
        .maxLength(512)
        .enableKeyboardOnFocus(true)
        .enterKeyType(EnterKeyType.Send)
        .position({ x: 0, y: 0 })
        .onChange((value: string) => {
          this.handleImeInput(value);
        })
        .onSubmit(() => {
          // 输入法上的「发送 / 回车」：聊天框发送、告示牌换行都靠它
          // 先打个「别急着补焦点」的标记，再投 ENTER：
          // 输入法会自己先收起键盘，而引擎要到下一帧才把 want 置回 false，
          // 没有这个窗口就会「键盘沉下去又弹回来」。
          this.imeSuppressUntil = Date.now() + IME_SUBMIT_QUIET_MS;
          this.sendKeyTap(KEYCODE_ENTER);
          this.resetImeMirror();
        })
        .onFocus(() => {
          this.imeFocused = true;
        })
        .onBlur(() => {
          this.imeFocused = false;
        })

      Text(this.status)
        .fontSize(11)
        .fontFamily('monospace')
        .fontColor('#8CE99A')
        .backgroundColor('#80000000')
        .padding(4)
        .position({ x: 4, y: 4 })
        // 这行 fps / 状态只是「显示」，绝不能挡住它下面的画面：
        // 默认 HitTestMode 会把落在它范围内的触点自己吃掉，XComponent 收不到，
        // 于是左上角那一小块既不能挖方块也不能转视角。
        .hitTestBehavior(HitTestMode.None)
    }
    .id(this.containerId)               // 设置 ID 以便请求焦点
    .focusable(true)                    // 允许获得焦点
    .onKeyEvent((event: KeyEvent) => {
      if (DEBUG_KB) {
        hilog.info(LOG_DOMAIN, LOG_TAG, 'raw type=%{public}d code=%{public}d text=%{public}s held=%{public}d',
                   event.type, event.keyCode, event.keyText, this.heldKeys.size)
      }

      // CANCEL（值=3）：系统取消这次按键序列。它既可能跟在 Up 后面重复出现，
      // 也可能在双键混按 / 输入法插手时单独出现 —— 后者以前被直接丢弃，
      // 那个键就永远留在按住集合里、每帧被补按，表现正是「松开后角色自己一直走」。
      // 现在按「抬起」处理：若该键本来就在挂起中，onKeyUp 会忽略；
      // 否则走 90ms 去抖 —— 真要还在按，随后的 repeat Down 会立刻撤销，不会打断移动。
      // 当前 SDK 的 KeyType 类型尚无 CANCEL 枚举，故用字面量 3 判断。
      if (event.type === 3) {
        if (DEBUG_KB) {
          hilog.info(LOG_DOMAIN, LOG_TAG, 'raw CANCEL code=%{public}d held=%{public}d',
                     event.keyCode, this.heldKeys.size)
        }
        this.onKeyUp(event.keyCode)
        return true
      }

      // 去重长按 repeat：Down 时若该键已在按住集合中，则跳过（重复按下）。
      // Up 不立刻投递：移动键会挂起一小段时间（见 onKeyUp），
      // 期间若同键又按下就当作同一次长按 —— 这样按键事件成对抖动时
      // 也不会把引擎里的「按住」状态反复清掉（那正是「走一下、停一下」的来源）。
      // 只有明确的「抬起」才当成松开；其余类型（Down=0、以及其他系统扩展类型）
      // 一律按「按下」处理。这样即使系统用了非 0/1 的类型表示长按 repeat，
      // 也不会被误判成抬起把移动状态清掉（那是「按住前进却走走停停」的根因）。
      const isDown = (event.type !== KeyType.Up)

      // 仅在实际投递（非 repeat）时更新状态，避免高频 repeat 反复触发
      // @State 重渲染拖慢渲染循环。
      if (isDown && !this.heldKeys.has(event.keyCode)) {
      }

      if (isDown) {
        this.onKeyDown(event.keyCode, event.keyText)
      } else {
        this.onKeyUp(event.keyCode)
      }

      // 消费掉事件（返回 true），不让框架继续做默认处理：
      // 否则空格可能被当成「激活当前焦点组件」、方向键可能触发焦点导航，
      // 焦点一跑掉，后续按键就全都收不到了（表现就是「按了没反应/两个键不能一起按」）。
      return true
    })
    .width('100%')
    .height('100%')
    .backgroundColor('#101418')
    .expandSafeArea([SafeAreaType.SYSTEM], [SafeAreaEdge.TOP, SafeAreaEdge.BOTTOM])
  }
}
