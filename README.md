
---

# 智能农业管理系统

本项目作为挑战杯“人工智能+”创意赛“智农启航，慧控未来”项目组提交作品，展示内容如下。

## 演示视频

请前往观看演示视频：[点击这里查看演示视频](#)

## 模型文件

模型文件已托管于 Hugging Face 仓库：[Smart-Crop 模型](https://huggingface.co/aiyawanan1112/Smart-Crop)

---

## 项目背景

本项目立足于我国农业现代化与数字乡村建设的战略需求，紧扣国家“人工智能+智慧农业”发展方向，设计并初步实现了一套面向多种应用场景的智能农业管理系统。项目依托人工智能大模型技术，围绕环境数据实时感知、智能决策优化与自动控制执行三个核心环节，提出了具有普适性与可落地性的农业数字化升级整体解决方案，致力于打通农业生产中的“数据孤岛”与“智能瓶颈”，推动农业生产全面迈向智能化、精细化与绿色低碳的新阶段。

---

## 技术细节

### 数据集收集

数据集格式示例：

```json
{
  "instruction": "You are an experienced agronomist proficient in farming techniques, crop management, and disease-resistant crop cultivation, you are tasked with answering questions based on your expertise.",
  "input": "What are some methods for improving soil fertility?",
  "response": "Improving soil fertility can be achieved through various methods such as crop rotation, cover cropping, and adding organic matter like compost or manure. Additionally, using green manures, practicing conservation tillage, and applying appropriate fertilizers based on soil testing can help enhance soil fertility and promote healthier plant growth."
}
```

### 模型基座及微调平台

- **模型基座：** 采用 DeepSeek-R1-Distill-Qwen-7B 进行微调  
- **微调平台：** 考虑时间和微调大小，选择 3 张 4090 显卡进行并行训练

### 模型训练

采用 QLoRa 量化+DDP 联动 3 卡并行训练，主要代码如下：

```python
training_args = TrainingArguments(
    output_dir="./outputs",
    per_device_train_batch_size=2,
    per_device_eval_batch_size=2,
    num_train_epochs=3,
    learning_rate=2e-4,
    logging_steps=10,
    save_steps=50,
    save_total_limit=3,
    fp16=True,
    do_eval=True,
    eval_steps=50,
    gradient_accumulation_steps=4,
    warmup_steps=100,
    weight_decay=0.01,
    optim="adamw_torch",
    local_rank=local_rank,
    ddp_find_unused_parameters=False,
)

```

#### 启动训练

```bash
export CUDA_VISIBLE_DEVICES=0,1,2 python train.py
```

---

### 模型融合

融合 adapter 权重并保存完整模型的示例代码如下：

```python
import torch
from transformers import AutoModelForCausalLM, AutoTokenizer
from peft import PeftModel  # 请确保已安装 peft 库

# 1. 加载原始预训练模型（基座模型）
base_model_path = '/root/DeepSeek-R1-Distill-Qwen-7B'
model = AutoModelForCausalLM.from_pretrained(base_model_path)

# 2. 通过 peft 加载微调 adapter
# adapter_path 指向包含 adapter_config.json 和 adapter_model.safetensors 的目录（而非单个权重文件）
adapter_path = '/root/DeepSeek-R1-Distill-Qwen-7B/checkpoint-19000'
model = PeftModel.from_pretrained(model, adapter_path)

# 3. 将 adapter 权重融合到模型中
model = model.merge_and_unload()

# 4. 加载分词器
tokenizer = AutoTokenizer.from_pretrained(base_model_path)

# 5. 保存融合后的完整模型
save_directory = '/root/DeepSeek-R1-Distill-Qwen-7B/Diabetes_model'
model.save_pretrained(save_directory)
tokenizer.save_pretrained(save_directory)

print("Adapter 权重已融合，完整模型保存完毕！")
```

---

## 封装

将模型整合至前端，并封装为 APK 安装包（详见决策ai.apk）。演示视频链接请参见文档开头部分。

---

以上就是本项目的整体架构与技术细节说明，如有疑问欢迎交流讨论。
