from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from paddlespeech.cli.asr.infer import ASRExecutor
import os

# 禁用所有日志



# 初始化 ASR 执行器
app = FastAPI()
asr = ASRExecutor()


# 定义请求体模型
class AudioPathRequest(BaseModel):
    path: str  # 音频文件的本地路径


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

    # 返回识别结果
    return {"text": result}


if __name__ == "__main__":
    import uvicorn

    uvicorn.run(app, host="0.0.0.0", port=5000)
