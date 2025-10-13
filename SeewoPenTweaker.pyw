import time
import threading
from pynput import keyboard, mouse
from pystray import Icon, Menu, MenuItem
from PIL import Image, ImageDraw

class KeyListener:
    def __init__(self):
        self.last_p_time = 0  # 记录最后一次ctrl+shift+alt+p的时间
        self.is_left_pressed = False  # 左键是否处于按住状态
        self.mouse_controller = mouse.Controller()  # 鼠标控制器
        self.listening = True  # 监听状态标志
        self.delay = 0.3  # 延迟时间，单位：秒（300ms）
        self.press_timer = None  # 用于延迟执行的计时器
        # 新增：PageUp/PageDown长按定时器
        self.pageup_timer = None
        self.pagedown_timer = None
        self.keyboard_controller = keyboard.Controller()
        # 定义热键组合
        self.hotkey_p = keyboard.GlobalHotKeys({
            '<ctrl>+<shift>+<alt>+p': self.on_p_press
        })
        self.hotkey_q = keyboard.GlobalHotKeys({
            '<ctrl>+<shift>+<alt>+q': self.on_q_press
        })
        self.hotkey_win_d = keyboard.GlobalHotKeys({
            '<cmd>+d': self.simulate_win_d
        })
        # 创建托盘图标（可自定义图片）
        self.tray_icon = self.create_tray_icon()
        # 启动键盘监听器
        self.key_listener = keyboard.Listener(on_press=self.on_press, on_release=self.on_release)

    def create_tray_icon(self):
        """创建系统托盘图标，可自定义图片和菜单"""
        try:
            image = Image.open('icon.png')
        except:
            image = Image.new('RGB', (20, 20), color='blue')
            draw = ImageDraw.Draw(image)
            draw.rectangle([(5, 5), (15, 15)], fill='white')
        menu = Menu(
            MenuItem('状态: 运行中', lambda: None, enabled=False),
            MenuItem('自定义键位', self.open_key_config),
            MenuItem('重载配置', self.reload_config),
            MenuItem('退出', self.stop_listening)
        )
        return Icon("键鼠转换工具", image, "键鼠转换工具", menu)

    def open_key_config(self):
        import tkinter as tk
        from tkinter import simpledialog
        root = tk.Tk()
        root.withdraw()
        result = simpledialog.askstring('自定义键位', '请输入新的Win+D热键（如<cmd>+d）：')
        if result:
            self.update_hotkey_win_d(result)
        root.destroy()

    def reload_config(self):
        print('配置已重载')

    def update_hotkey_win_d(self, hotkey_str):
        self.hotkey_win_d = keyboard.GlobalHotKeys({hotkey_str: self.simulate_win_d})

    def _actually_press_left(self):
        """实际执行左键按住的函数（延迟后调用）"""
        if self.listening and not self.is_left_pressed:
            self.mouse_controller.press(mouse.Button.left)
            self.is_left_pressed = True

    def on_p_press(self):
        """处理ctrl+shift+alt+p组合键，添加300ms延迟"""
        current_time = time.time() * 1000  # 转换为毫秒
        self.last_p_time = current_time
        
        # 如果已有计时器，先取消
        if self.press_timer:
            self.press_timer.cancel()
            
        # 创建新的延迟计时器
        self.press_timer = threading.Timer(self.delay, self._actually_press_left)
        self.press_timer.start()

    def on_q_press(self):
        """处理ctrl+shift+alt+p组合键"""
        # 如果有等待中的左键按下计时器，先取消
        if self.press_timer:
            self.press_timer.cancel()
            self.press_timer = None
        
        current_time = time.time() * 1000
        # 检查200ms内是否收到过ctrl+shift+alt+p
        if self.last_p_time > 0 and (current_time - self.last_p_time) <= 200:
            # 先松开可能按住的左键
            if self.is_left_pressed:
                self.mouse_controller.release(mouse.Button.left)
                self.is_left_pressed = False
            # 模拟右键点击
            self.mouse_controller.click(mouse.Button.right, 1)
            self.last_p_time = 0  # 重置计时
        else:
            # 单独收到q组合键，松开左键
            if self.is_left_pressed:
                self.mouse_controller.release(mouse.Button.left)
                self.is_left_pressed = False
            self.last_p_time = 0  # 重置计时

    def on_press(self, key):
        pass

    def on_release(self, key):
        pass

    # 已删除Win+Shift+S截图功能
    # 已删除鼠标中键点击功能

    def simulate_win_d(self):
        # 模拟Win+D组合键
        self.keyboard_controller.press(keyboard.Key.cmd)
        self.keyboard_controller.press('d')
        self.keyboard_controller.release('d')
        self.keyboard_controller.release(keyboard.Key.cmd)

    def start_listening(self):
        """开始监听键盘事件"""
        threading.Thread(target=self.tray_icon.run, daemon=True).start()
        self.key_listener.start()
        with self.hotkey_p, self.hotkey_q, self.hotkey_win_d:
            while self.listening:
                time.sleep(0.1)

    def stop_listening(self):
        """停止监听并退出程序"""
        self.listening = False
        # 取消可能存在的计时器
        if self.press_timer:
            self.press_timer.cancel()
        # 确保退出前释放所有按键
        if self.is_left_pressed:
            self.mouse_controller.release(mouse.Button.left)
        # 停止托盘图标
        self.tray_icon.stop()
        print("程序已退出")

if __name__ == "__main__":
    try:
        print("首次运行请先安装依赖：")
        print("pip install pynput pystray pillow")
        print("程序已启动，托盘图标已添加到系统托盘区")
        listener = KeyListener()
        listener.start_listening()
    except Exception as e:
        print(f"程序出错: {e}")
