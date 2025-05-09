from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from paddlespeech.cli.asr.infer import ASRExecutor
import llm
import os
import uvicorn
import yaml


app = FastAPI()
# 初始化 ASR 执行器
asr = ASRExecutor()
# 获取当前脚本所在目录
base_dir = os.path.dirname(os.path.abspath(__file__))
config_path = os.path.join(base_dir, "config.yaml")


# 定义请求体模型
class AudioPathRequest(BaseModel):
    path: str  # 音频文件的本地路径

class TextRequest(BaseModel):
    text: str  # 需要分类的文本

@app.post("/asr")
async def asr_service(request: AudioPathRequest):
    # 获取音频文件路径
    audio_path = request.path

    # 检查文件是否存在
    if not os.path.exists(audio_path):
        raise HTTPException(status_code=400, detail="Audio file not found")

    # 调用 ASR 模型进行语音转文字
    try:
        result = asr(audio_file=audio_path, model="conformer_online_wenetspeech")
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

    # 使用 llm 润色文本(修改错别字，添加标点)
    try:
        result = llm.process_asr_output(result)
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"LLM processing failed: {str(e)}")

    # 返回识别结果
    return {"text": result}

@app.post("/classify")
async def classify_service(request: TextRequest):
    # 获取文本
    text = request.text

    # 调用 LLM 进行文本分类
    try:
        result = llm.classify_text(text)
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"LLM processing failed: {str(e)}")

    # 返回分类结果
    return {"classification": result}

if __name__ == "__main__":
    with open(config_path, "r", encoding="utf-8") as f:
        config = yaml.safe_load(f)
        uvicorn.run(app, host="0.0.0.0", port=config['port'])
