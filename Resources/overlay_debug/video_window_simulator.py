import tkinter as tk
import argparse

class VideoWindowSimulator:
    def __init__(self, window_title="Video Player", width=640, height=480):
        self.root = tk.Tk()
        self.root.title(window_title)
        self.root.geometry(f"{width}x{height}")

        # Add position and size label
        self.info_label = tk.Label(self.root, text="")
        self.info_label.pack(pady=10)

        # Update info periodically
        self.update_info()

        # Make window resizable
        self.root.resizable(True, True)

    def update_info(self):
        # Get window position and size
        x = self.root.winfo_x()
        y = self.root.winfo_y()
        width = self.root.winfo_width()
        height = self.root.winfo_height()

        # Update label
        self.info_label.config(
            text=f"Position: {x}, {y}\nSize: {width} x {height}"
        )

        # Schedule next update
        self.root.after(100, self.update_info)

    def run(self):
        self.root.mainloop()

def main():
    parser = argparse.ArgumentParser(description='Video Window Simulator')
    parser.add_argument('--title', default="Video Player",
                      help='Window title (default: Video Player)')
    parser.add_argument('--width', type=int, default=640,
                      help='Initial window width (default: 640)')
    parser.add_argument('--height', type=int, default=480,
                      help='Initial window height (default: 480)')

    args = parser.parse_args()

    simulator = VideoWindowSimulator(
        window_title=args.title,
        width=args.width,
        height=args.height
    )
    simulator.run()

if __name__ == "__main__":
    main()
