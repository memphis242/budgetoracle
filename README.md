# Budget Oracle
## How It'll Look (Most of the Time)

```bash
> budget_oracle
Welcome, welcome, my child.
Today's Date: 12-31-2025

budget_oracle > print budget 1 in 10 days
Balance: $820.25

budget_oracle > print budget 1 up to 10 days from now
Today's Date: 12/30/2025
Today's Balance: $1021.58

12/31/2025: $1021.58
01/01/2026: $ 801.20
01/02/2026: $ 735.88
01/03/2026: $ 735.88
01/04/2026: $ 735.88
01/05/2026: $ 689.01
01/06/2026: $ 458.57
01/07/2026: $1858.57
01/08/2026: $1842.98
01/09/2026: $ 820.25

budget_oracle > can i spend $450 on 01/25/2026?
Yes!
- Balance on 01/25/2026 as things stand pre-transaction would be: $1041.21, 
- Balance after transaction: $591.21
- Budget will be in-tact through to the next month, remaining above pre-configured minimum of $150.00

budget_oracle > can i spend $450 on 01/25/2026 while remaining whole through to 03/15/2026[?]
No. Your budget minimum will be breached on 03/05/2026.
- Balance going into 03/05/2026: $201.50
- Transactions on that day:
    - $21.99 for "subscription X"
    - $50.00 for "planned purchase of item Y"
- Balance after: $129.51, which is $20.49 below the pre-configured minimum of $150.00

budget_oracle > what transactions are set for 02/21/2026?
None.

budget_oracle > what transactions are set for 03/01/2026?
- $9.99 for "subscription X"
- $100.00 for "loan Y"
```

## Motivation
There are lots of apps out there that capture projections on your full financial state, accounting for investments, savings, debt, etc. This app is not that. I just wanted it to be a **local-only**, **secure**, **lightweight** way of checking what an account's balance will be in X time, or to ask the question `can I purchase item X of $Y on date MM/DD/YYYY?`.

I also wanted it to be terminal-based, because that's my style (I'm 100% a terminal junkie).

So, what is the minimum information needed to deliver this?

1. an account's current balance
2. a set of expected future transactions (just the monetary amounts for each deposit/withdrawal)

Your account balance is easy enough for _you_ to look up, so this app makes no business of looking that up on your behalf. You will enter it in manually, _without specifying any account credentials_ - just the balance. The set of expected transactions on the other hand... is a lot more tedious to manually enter in every time. So, this is the only piece of information that the app will pesist for you _after_ you enter it in once manually. There is a lot of flexibility in the type of transaction (regular, one-off, what-if, etc.), and since this information is sensitive, it will be stored encrypted by default, using a password you define.
