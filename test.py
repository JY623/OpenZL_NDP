import os
import torch
import shutil
from transformers import AutoModelForCausalLM, AutoTokenizer
from safetensors.torch import save_file, load_file

# 1. Configuration
model_id = "meta-llama/Meta-Llama-3-8B"
temp_dir = "./llama3-8b-sharded-300mb"      # Temporary directory for safetensors
output_dir = "./llama3-8b-pt-shards"         # Final directory for OpenZL-compatible .pt shards
MAX_SHARD_SIZE = 300 * 1024 * 1024           # 300MB in bytes
MAX_FILES = 10                               # Limit the number of shards to 10

os.makedirs(temp_dir, exist_ok=True)
os.makedirs(output_dir, exist_ok=True)

# 2. Load Model
print(f"--- Starting model load: {model_id} ---")
model = AutoModelForCausalLM.from_pretrained(
    model_id,
    torch_dtype=torch.float16,
    device_map="auto",
    low_cpu_mem_usage=True
)

# 3. Extract Weights and Shard (Max 10 files)
print(f"--- Starting weight extraction and sharding (Limit: {MAX_FILES} files) ---")
state_dict = model.state_dict()
current_shard = {}
current_size = 0
shard_count = 0

for key, tensor in state_dict.items():
    if shard_count >= MAX_FILES:
        break
        
    # Calculate tensor size in bytes
    tensor_size = tensor.nelement() * tensor.element_size()
    
    # Save shard if it exceeds the maximum size
    if current_size + tensor_size > MAX_SHARD_SIZE and current_shard:
        shard_count += 1
        shard_name = f"model-{shard_count:05d}.safetensors"
        temp_path = os.path.join(temp_dir, shard_name)
        
        # A. Save as temporary safetensors
        save_file(current_shard, temp_path)
        
        # B. Immediately convert to PyTorch (.pt) format
        pt_path = os.path.join(output_dir, shard_name.replace(".safetensors", ".pt"))
        weights = load_file(temp_path)
        torch.save(weights, pt_path)
        
        print(f"[{shard_count}/{MAX_FILES}] Conversion complete: {pt_path}")
        
        # C. Remove original safetensors to save disk space
        os.remove(temp_path)
        
        # Reset for next shard
        current_shard = {}
        current_size = 0
    
    current_shard[key] = tensor
    current_size += tensor_size

# 4. Final Cleanup
if os.path.exists(temp_dir):
    shutil.rmtree(temp_dir)
    print(f"--- Temporary directory {temp_dir} removed ---")

print(f"--- All tasks completed! Check the directory: {output_dir} ---")