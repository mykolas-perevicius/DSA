class Solution:
    def uniquePaths(self, m: int, n: int) -> int:
        # Total moves needed: (m-1) down and (n-1) right
        total_moves = m + n - 2
        # Choose the smaller dimension to minimize the number of multiplicative steps
        choose = min(m - 1, n - 1)
        
        result = 1
        for i in range(1, choose + 1):
            result = result * (total_moves - choose + i) // i
        return result

# Combinatorial Insight: The robot's path consists of (m-1) down moves and (n-1) right moves. The number of ways to arrange these moves is given by the combination formula C(total_moves, choose), where total_moves = m + n - 2 and choose = min(m-1, n-1).

# Step-by-Step Calculation: Instead of directly computing factorials (which can be very large), we compute the combination iteratively. This approach multiplies the necessary terms and divides step-by-step to maintain integer precision and avoid overflow.

# Efficiency: The algorithm runs in O(k) time where k is the minimum of (m-1, n-1), making it highly efficient even for the upper constraint values (up to 100).

