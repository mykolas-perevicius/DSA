class Solution:
    def maxSubArray(self, nums: List[int]) -> int:
        # Initialize current_sum and max_sum with the first element
        current_sum = max_sum = nums[0]
        
        # Iterate through the array starting from the second element
        for num in nums[1:]:
            # Decide whether to start a new subarray at the current element
            # or to add the current element to the previous subarray
            current_sum = max(num, current_sum + num)
            # Update the maximum subarray sum found so far
            max_sum = max(max_sum, current_sum)
        
        return max_sum        

# Initialization: Start by setting both current_sum and max_sum to the value of the first element in the array. This handles the base case where the array has only one element.

# Iteration: Loop through the array starting from the second element. For each element:

# Update current_sum: Calculate the maximum sum of the subarray ending at the current position. This is the maximum of the current element itself (starting a new subarray) or the sum of the current element and the previous current_sum (extending the previous subarray).

# Update max_sum: Check if the new current_sum is greater than the existing max_sum and update max_sum if it is.

# Result: After processing all elements, max_sum holds the maximum subarray sum.

# Time Complexity
# The time complexity of this approach is O(n), where n is the length of the input array. This is because we traverse the array exactly once, performing constant-time operations at each step.

# Space Complexity
# The space complexity is O(1) since we only use a few additional variables to keep track of the sums, regardless of the input size.