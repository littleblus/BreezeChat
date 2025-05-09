import requests
import yaml
import os
from urllib.parse import urljoin

# 获取当前脚本所在目录
base_dir = os.path.dirname(os.path.abspath(__file__))
config_path = os.path.join(base_dir, "config.yaml")

def process_asr_output(asr_text):
    # 读取 YAML 文件
    with open(config_path, "r", encoding="utf-8") as f:
        config = yaml.safe_load(f)

    prompt = f"""/no_think
    以下是一段没有标点的语音识别结果，请你直接输出其润色后的版本（忠实原意、通顺流畅），不要解释或分析，只给最终结果。

    原句：{asr_text}
    润色后："""

    endpoint = "api/generate"
    response = requests.post(
        urljoin(config['api'], endpoint),
        json={
            "model": config['model'],
            "prompt": prompt,
            "stream": False
        }
    )

    if response.status_code == 200:
        result = response.json()
        return result.get("response", "").split("</think>")[-1].strip()
    else:
        raise Exception(f"Ollama 接口请求失败，状态码：{response.status_code}, 响应内容：{response.text}")


def classify_text(text):
    # 读取 YAML 文件
    with open(config_path, "r", encoding="utf-8") as f:
        config = yaml.safe_load(f)

    prompt = f"""/no_think
    以下是一段用户昵称文本，请你判断该昵称是否涉及不当或敏感内容，包括但不限于以下类别：政治相关（含政治人物姓名、谐音、恶搞）、色情低俗、暴力、辱骂、违法用语、敏感词变体或谐音。请你综合考虑其**谐音、隐喻、网络黑话或变体拼写**等方式是否规避审核。
    请直接输出分类结果：合规 / 不合规，不要解释或分析，不要输出其他任何内容。你的回答必须是“合规”或“不合规”。

    文本：{text}
    分类结果："""

    endpoint = "api/generate"
    response = requests.post(
        urljoin(config['api'], endpoint),
        json={
            "model": config['model'],
            "prompt": prompt,
            "stream": False
        }
    )

    if response.status_code == 200:
        result = response.json()
        result = result.get("response", "").split("</think>")[-1].strip()
        if "不合规" in result:
            return "不合规"
        elif "合规" in result:
            return "合规"
        else:
            raise Exception(f"分类结果不明确，请检查模型输出: {result}")
    else:
        raise Exception(f"Ollama 接口请求失败，状态码：{response.status_code}, 响应内容：{response.text}")


if __name__ == "__main__":
    text = str(input())
    print(classify_text(text))
