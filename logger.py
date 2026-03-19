import logging
import os


class Logger(object):
    def __init__(
        self,
        log_dir,
        log_name,
    ):
        os.makedirs(log_dir, exist_ok=True)
        log_path = os.path.join(log_dir, log_name)

        self.logger = logging.getLogger(f"{__name__}.{log_name}")
        self.logger.setLevel(logging.INFO)
        self.logger.propagate = False  # 避免被 root logger 重复输出

        # 防止重复添加 handler
        if self.logger.handlers:
            return

        class ColorFormatter(logging.Formatter):
            RED = "\033[1;31m"
            GREEN = "\33[1;32m"
            YELLOW = "\033[1;33m"
            RESET = "\033[0m"

            def _set_color(self, s, color):
                return f"{color}{s}{self.RESET}"

            def format(self, record):
                level = record.levelname
                if level == "INFO":
                    record.levelname = f"{self.GREEN}[INFO   ]{self.RESET}"
                elif level == "WARNING":
                    record.levelname = f"{self.YELLOW}[WARNING]{self.RESET}"
                elif level == "ERROR":
                    record.levelname = f"{self.RED}[ERROR  ]{self.RESET}"

                return super().format(record)

        formatter = ColorFormatter("%(levelname)s: %(message)s")

        # 文件输出：默认追加；显式指定 encoding，避免中文/特殊字符问题
        fh = logging.FileHandler(log_path, mode="a", encoding="utf-8")
        fh.setLevel(logging.INFO)
        fh.setFormatter(formatter)
        self.logger.addHandler(fh)

        # 控制台输出
        ch = logging.StreamHandler()
        ch.setLevel(logging.INFO)
        ch.setFormatter(formatter)
        self.logger.addHandler(ch)

    def info(self, message):
        if self.logger:
            self.logger.info(message)

    def warning(self, message):
        if self.logger:
            self.logger.warning(message)

    def error(self, message):
        if self.logger:
            self.logger.error(message)


if __name__ == "__main__":
    home_path = os.getcwd()
    logger = Logger(f"{home_path}/runs", "test.log")
    logger.info("This message is for info testing...")
    logger.warning("This message is for warning testing...")
    logger.error("This message is for error testing...")
