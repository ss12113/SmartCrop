import os
import torch
from transformers import (
    AutoTokenizer, 
    AutoModelForCausalLM, 
    Trainer, 
    TrainingArguments, 
    BitsAndBytesConfig, 
    DataCollatorForLanguageModeling
)
from datasets import load_dataset, concatenate_datasets
from peft import LoraConfig, get_peft_model

# 设置 CUDA 内存分配策略
os.environ["PYTORCH_CUDA_ALLOC_CONF"] = "expandable_segments:True"

model_name = "/root/DeepSeek-R1-Distill-Qwen-7B"

# 8-bit 量化配置
quantization_config = BitsAndBytesConfig(
    load_in_8bit=True,
    llm_int8_threshold=6.0,
)

# 加载分词器和模型（使用 int8 量化）
tokenizer = AutoTokenizer.from_pretrained(model_name)
tokenizer.pad_token = tokenizer.eos_token  # 确保有 padding token

# 获取当前进程对应的 GPU（多卡训练时，torchrun 会自动传入 LOCAL_RANK）
local_rank = int(os.environ.get("LOCAL_RANK", 0))

# 根据 LOCAL_RANK 手动将模型加载到对应 GPU 上
model = AutoModelForCausalLM.from_pretrained(
    model_name,
    quantization_config=quantization_config,
    low_cpu_mem_usage=True,
    device_map={"": local_rank}
)

# -------------------------
# 多数据集加载示例
# -------------------------
data_files = [
    "/root/DeepSeek-R1-Distill-Qwen-7B/1.jsonl",
    "/root/DeepSeek-R1-Distill-Qwen-7B/2.jsonl",
    "/root/DeepSeek-R1-Distill-Qwen-7B/3.jsonl",
]

datasets_list = [load_dataset('json', data_files=path, split="train") for path in data_files]
full_dataset = concatenate_datasets(datasets_list)

print("数据集示例:", full_dataset[0])
print("数据集字段:", full_dataset.column_names)

# -------------------------
# 数据预处理：分词和标签复制
# -------------------------
def tokenize_function(examples):
    # 检查输入字段，优先选择 'input' 或 'text'，否则遍历可能的字段
    input_field = "input" if "input" in examples else "text"
    if input_field not in examples:
        possible_fields = ["instruction", "content", "prompt", "source"]
        for field in possible_fields:
            if field in examples:
                input_field = field
                break
        else:
            raise ValueError(f"无法找到输入文本字段。可用字段: {list(examples.keys())}")
    
    texts = examples[input_field]
    tokenized = tokenizer(texts, padding="max_length", truncation=True, max_length=512)
    return tokenized

full_dataset = full_dataset.map(tokenize_function, batched=True)

def add_labels(example):
    example["labels"] = example["input_ids"].copy()
    return example

full_dataset = full_dataset.map(add_labels, batched=False)

# 划分训练集和验证集（验证集占 5%）
split_dataset = full_dataset.train_test_split(test_size=0.05, seed=42)
train_dataset = split_dataset["train"]
eval_dataset = split_dataset["test"]

print(f"训练集大小: {len(train_dataset)}")
print(f"验证集大小: {len(eval_dataset)}")

# -------------------------
# LoRA 配置及模型封装
# -------------------------
lora_config = LoraConfig(
    r=8,
    lora_alpha=32,
    lora_dropout=0.1,
    target_modules=["q_proj", "k_proj", "v_proj", "o_proj"],
    bias="none",
    task_type="CAUSAL_LM",
)

peft_model = get_peft_model(model, lora_config)
peft_model.print_trainable_parameters()

# -------------------------
# 多卡训练配置
# -------------------------
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

data_collator = DataCollatorForLanguageModeling(
    tokenizer=tokenizer,
    mlm=False
)

def compute_metrics(eval_preds):
    import numpy as np
    logits, labels = eval_preds
    mask = labels != -100
    labels_masked = labels[mask]
    predictions = np.argmax(logits, axis=-1)
    predictions_masked = predictions[mask]
    correct = (predictions_masked == labels_masked)
    accuracy = correct.mean()
    return {
        "accuracy": float(accuracy),
        "samples": int(len(labels_masked))
    }

trainer = Trainer(
    model=peft_model,
    args=training_args,
    train_dataset=train_dataset,
    eval_dataset=eval_dataset,
    compute_metrics=compute_metrics,
    data_collator=data_collator
)

def main():
    print("开始训练...")
    # 使用 resume_from_checkpoint 参数，从指定检查点恢复训练
    resume_checkpoint = "./outputs/checkpoint-15550"
    trainer.train(resume_from_checkpoint=resume_checkpoint)
    
    print("训练完成，保存模型...")
    trainer.save_model("./final_model")
    print("模型已保存到 ./final_model")
    
    try:
        print("开始最终评估...")
        final_metrics = trainer.evaluate()
        print(f"最终评估结果: {final_metrics}")
    except Exception as e:
        print(f"评估过程出错: {e}")
        print("模型仍已保存")

if __name__ == "__main__":
    main()
