//< lab02     ** machine mode -> supervisor mode -> user mode **
(1)
    * 其实很多kernel的代码、函数也不是说有多基础的， kernel和user的代码没什么区别的，区别是在于两者所处的模式不同而已
    * 模式的切换和选择，我还不是很清楚，不知道怎么搞定，只知道一个syscall，而其实这个说到底也不过是一个切换函数的中介
(2)
    * 代码为每个process分配空间，每个空间大小为PGSIZE，由一个struct run* 存放地址
    * 剩余的空余空间由一个全局指针指着，相当于空间的alloc由这个指针管着
    * 每个process的结构体，状态都封装好变成一些结构体或数组了
(3)
    * 还有很多东西，像lock, release都不懂，应该没学过
    * C代码如何结合在一起，而后变成一个底层的控制CPU和其他硬件资源分配的功能，不是很清楚；
    * 我目前的理解是：
        * kernel管控着CPU的资源，尤其是一些敏感硬件资源，如memory,register,cache
        * 而user只能使用基本的ALU类似的资源
        * 此外，kernel还起着分配各process对硬件使用的次序的问题； 因而其凭空创造了一些像process,thread,lock这些比较高层的东西
        * 简而言之，user只有很小的权利，而kernel有很强大的priviledge，所以造成了user听命于他，即使原本自己所属的权利也或多或少受着kernel的限制
        * 当然，这些还是猜测，得快些学些东西，不然越来越糊涂
#4.2 19:55

