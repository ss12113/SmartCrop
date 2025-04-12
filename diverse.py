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
