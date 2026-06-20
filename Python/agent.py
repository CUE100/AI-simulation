import torch
from torch import nn
import numpy as np
import torch.optim as optim
from torch.nn import LSTM, GRU
from torch.nn import Conv2d, MaxPool2d
import torch.nn.functional as F
from torch.utils.data import DataLoader, Dataset
device = torch.device("mps" if torch.backends.mps.is_available() else "cpu")


class AIbrain(nn.Module):
    def __init__(self):
        super().__init__()
        self.visual_layer = nn.Conv2d(
            in_channels=3, out_channels=16, kernel_size=3, padding=1)
        self.sound_layer = nn.Linear(in_features=4, out_features=8)
        self.sense_layer = nn.Linear(in_features=3, out_features=8)
        self.chemical_layer = nn.Linear(in_features=2, out_features=8)
        self.homeostasis_layer = nn.Linear(in_features=4, out_features=8)

        self.decision_core = nn.Sequential(
            nn.Linear(in_features=(16 * 32 * 32) +
                      8 + 8 + 8 + 8, out_features=64),
            nn.ReLU(),
            nn.Linear(in_features=64, out_features=4)
        )

    def visual(self, map_data):
        matrix_array = np.array(map_data, dtype=np.float32)
        visual_tensor = torch.tensor(matrix_array, dtype=torch.float32)
        visual_tensor = visual_tensor.permute(2, 0, 1)
        final_visual_matrix = visual_tensor.unsqueeze(0)
        return final_visual_matrix

    def sound(self, audio_frequencies):
        raw_list = list(audio_frequencies)
        numpy_vector = np.array(raw_list, dtype=np.float32)
        sound_tensor = torch.tensor(numpy_vector, dtype=torch.float32)
        final_sound_vector = sound_tensor.unsqueeze(0)
        return final_sound_vector

    def sense(self, tactile_data):
        numpy_vector = np.array(tactile_data, dtype=np.float32)
        sense_tensor = torch.tensor(numpy_vector, dtype=torch.float32)
        final_sense_vector = sense_tensor.unsqueeze(0)
        return final_sense_vector

    def chemical(self, chemical_data):
        numpy_vector = np.array(chemical_data, dtype=np.float32)
        chemical_tensor = torch.tensor(numpy_vector, dtype=np.float32)
        final_chemical_vector = chemical_tensor.unsqueeze(0)
        return final_chemical_vector

    def homeostasis(self, internal_metrics):
        numpy_vector = np.array(internal_metrics, dtype=np.float32)
        homeostasis_tensor = torch.tensor(numpy_vector, dtype=torch.float32)
        final_homeostasis_vector = homeostasis_tensor.unsqueeze(0)
        return final_homeostasis_vector

    def forward(self, v_tensor, s_tensor, t_tensor, c_tensor, h_tensor):

        x_vis = F.relu(self.visual_layer(v_tensor))
        x_vis = torch.flatten(x_vis, start_dim=1)

        x_snd = F.relu(self.sound_layer(s_tensor))
        x_sns = F.relu(self.sense_layer(t_tensor))
        x_chm = F.relu(self.chemical_layer(c_tensor))
        x_hmo = F.relu(self.homeostasis_layer(h_tensor))

        fused_thoughts = torch.cat((x_vis, x_snd, x_sns, x_chm, x_hmo), dim=1)

        raw_scores = self.decision_core(fused_thoughts)

        return F.softmax(raw_scores, dim=-1)
