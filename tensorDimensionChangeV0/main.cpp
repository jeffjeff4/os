#include <torch/torch.h>
#include <iostream>

int main() {
    // Create a 3D tensor of shape [2, 3, 4]
    torch::Tensor input = torch::arange(2 * 3 * 4).reshape({2, 3, 4});
    std::cout << "Original tensor:\n" << input << "\n";

    // Get original sizes
    auto sizes = input.sizes();  // [2, 3, 4]
    int D0 = sizes[0]; // batch
    int D1 = sizes[1]; // second dim
    int D2 = sizes[2]; // third dim

    // Create output tensor with shape [2, 4, 3]
    torch::Tensor output = torch::empty({D0, D2, D1}, input.options());

    // Manually copy values
    for (int i = 0; i < D0; ++i) {
        for (int j = 0; j < D1; ++j) {
            for (int k = 0; k < D2; ++k) {
                // Swap j and k
                output[i][k][j] = input[i][j][k];
            }
        }
    }

    std::cout << "\nAfter swapping dim 1 and 2:\n" << output << "\n";

    return 0;
}
