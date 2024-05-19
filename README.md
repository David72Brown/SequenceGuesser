# SequenceGuesser
Program which guesses the next number in a sequence provided by the user.
(I use the term sequence and series interchangeably below.)

The idea is that the user provides a sequence of integers and the program figures out the nature of the sequence and is therefore able to
predict the next number in the sequence.

E.g.
User input			Next number		Reason
1, 2, 3, 4			5				Natural numbers i.e. S(i) = i+1 (since i goes from 0).
1, 2, 3, 5, 8, 13	21				Fibonnaci numbers i.e. S(i) = S(i-1) + S(i-2).
5, 18, 317			100482			S(i) = S(i-1)^2 - 7. (The ^2 means the number is squared.)

Coding Approach
---------------
Whatever the formula is (i.e. S(i) = whatever) it will be a string of up to, say, 20 chars and will be some permutation of the following characters:
	0-9
	+-*/
	square
	cube
	square root

If we go through every possible permutation, we will encounter a string which contains the correct expression for S(i) that we're looking for.
Each permutation is a string which might or might not be mathematically valid e.g. "3*S(i-1)+6" is valid but "++S((" isn't. So we can quickly
discard permutations which aren't valid. For valid ones, we parse them and use them to mathematically generate the numbers from the start of
the sequence provided by the user. If the formula correctly generates all of the user's numbers then we assume it's correct and we use it to
generate the next number, which we give to the user as the answer.
Brackets might be a nuisance to check and parse so the whole thing works using Reverse Polish notation. (I won't describe that here but you
can google it for a description.)
The number of permutations of numbers and operators is astronomical so we use various methods of reducing it. E.g. the number of operations
and operands in the string must be such that by the end of the string, we have exactly 1 number left on the stack. (Bear in mind how Reverse
Notation works by pushing operands onto the stack then whenever we encounter an operation, we pop the arguments off the stack, perform the
operation and push the result back onto the stack.)
We can also rule out various combinations of operations/operands. Two examples are below:
1. If we encounter "1 2 +" in a string, we can discard it because at some later point, we will check a different string which is exactly the same
but with "3" in the position where "1 2 +" is. E.g. we don't need to consider that S(i) equates to "S(i-1) 1 2 + *" since we will find the right
answer when we consider "S(i-1) 3 *".
2. If we encounter "S(i-1) i 1 + +", we can discard it because at some point we will also check "S(i-1) i + 1 +" which is equivalent.

Seeds
-----
This is a tricky issue. Consider the Fibonacci series: you can't define it just by saying S(i) = S(i-1) + S(i-2) because, according to that
definition, this is a Fibonacci series:
	0, 0, 0, 0, 0, 0, ...
	As is this:
	55, 2, 57, 59, 116, ...
The full definition of the Fibonacci series is:
	S(0) = 1
	S(1) = 2
	For all i > 1, S(i) = S(i-1) + S(i-2)
So the initial 2 numbers (1 and 2) are seeds which start the series off but which don't follow the definition used for all subsquent numbers
in the series.
This idea of seeds is a problem for us. Consider the sequence below:
4 6 13 19 40 58
The answer is that S(i) equates to "S(i-2) 3 * 1 +" (i.e. each number is 3 times the number 2 before it, plus 1.)
When we look at the candidate formula "S(i-2) 3 * 1 +" we see that it has a "S(i-2)" in it, which tells us that we can't expect it to correctly
generate the 1st or 2nd numbers in the sequence (i.e. the 4 or 6) since they would have to just be seeds in the sequence. So we skip those and
just check that the 3rd and subsequent numbers are generated correctly.
But what if the user gives us a short sequence which is mostly just seeds?
1 3 6 19 22
The formula we're looking for could be S(i) is "S(i-4) 21 +" and the first 4 numbers are seeds. But it could be that the correct formula is
"S(i-4) 2 * 20 +". We don't know because, since all but the last number provided by the user are seeds, we've only got the 22 to go on.
This could result in unintuitive answers from the program e.g. the user provided this sequence:
1, 2, 3, 4
They are expecting the program to tell it that the next number should be 5, but the program might decide that S(i) = "S(i-3) 3 +" because when
it checked that candidate string, it noted the reference to "S(i-3)", dismissed the first 3 numbers as seeds, and checked the only remaining
number (the 4) and found that its candidate formula was correct. If the program kept on checking for further possible correct formulae, it would
indeed find "S(i-1) 1 +" which would be correct and requires only 1 seed, and it would also find "i 1 +" which would also be correct and doesn't
require any seeds at all so every number in the sequence can be checked for complete confidence in the formula.
So the program initially aims to find a correct formula which requires no seeds, i.e. it doesn't refer to any "S(i-k)" values. If it fails, it
tries again but allowing itself to only include "S(i-1)" i.e. just allowing for a single seed value. If that fails, it allows "S(i-2)" in the
string, then S(i-3) and so on. When it finds an answer (assuming it does) then it knows it has the answer with the minimum possible seeds and
therefore the maximum possible confidence that this was the answer the user was looking for.

Improvements to code
--------------------
The code contains some commented-out lines which would enable it to check individual digits of numbers in the sequence, so it could solve
things like this:
86 14 5 5 5
Each number is the sum of the digits of the previous number, so the next number (and all subsequent numbers) would be 5.
The problem is that with every extra mathematical operation we add, the number of permutations of formulae to go through increases exponentially,
so solving each the most simple sequences (e.g. 1, 2, 3, 4) takes much longer. So for now they will probably stay commented out.
